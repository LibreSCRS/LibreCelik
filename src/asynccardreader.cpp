// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "asynccardreader.h"

#include "utils/libreceliklog.h"

#include <LibreSCRS/Auth/ErrorKeys.h>
#include <LibreSCRS/Plugin/CardPlugin.h>
#include <LibreSCRS/Secure/String.h>

#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QStringList>

#include <utility>

AsyncCardReader::AsyncCardReader(std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> candidates,
                                 std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> allPlugins,
                                 std::shared_ptr<LibreSCRS::SmartCard::CardSession> session, QObject* parent)
    : QObject(parent), candidates(std::move(candidates)), allPlugins(std::move(allPlugins)), session(std::move(session))
{
    QStringList ids;
    ids.reserve(static_cast<qsizetype>(this->candidates.size()));
    for (const auto& p : this->candidates)
        ids << QString::fromStdString(p->pluginId());
    qCDebug(lcProbeQuieting) << "LC-ASYNC-INIT candidates=" << this->candidates.size() << "pluginIds=["
                             << ids.join(QLatin1Char(',')) << "]";
    qRegisterMetaType<LibreSCRS::Plugin::CardData>("LibreSCRS::Plugin::CardData");
    qRegisterMetaType<LibreSCRS::Plugin::CardFieldGroup>("LibreSCRS::Plugin::CardFieldGroup");
    qRegisterMetaType<std::vector<LibreSCRS::Plugin::CertificateData>>(
        "std::vector<LibreSCRS::Plugin::CertificateData>");
    qRegisterMetaType<EmrtdCredentials>("EmrtdCredentials");
}

AsyncCardReader::~AsyncCardReader()
{
    cancel();
}

void AsyncCardReader::cancel()
{
    // CardSession (public API) does not expose SCardCancel; workers now rely
    // on the stopRequested flag checked between operations. This is not a
    // regression: the PC/SC spec does not specify SCardCancel as affecting
    // SCardTransmit on any platform (pcsc-lite, WinSCard, macOS
    // CryptoTokenKit) — SCardCancel targets SCardGetStatusChange. Cooperative
    // cancellation via stopRequested is therefore the only effective
    // mechanism regardless of platform.
    //
    // LM 4.0 hardening: we also request_stop on the stop_source so the
    // plugin's NVI readCard/sign wrapper observes the cancellation between
    // APDU groups (Plan A T7 step 7.4). Plugins that poll the token can
    // surface a SignResultOutcome::Cancelled / ReadResult::Cancelled
    // without waiting for the next blocking transmit to time out.
    stopRequested = true;
    stopSource.requestCancel();
    waitForPendingAsync();
}

void AsyncCardReader::initiateCancel()
{
    stopRequested = true;
    stopSource.requestCancel();
}

void AsyncCardReader::waitForPendingAsync()
{
    if (futureData.valid())
        futureData.wait();
    if (futurePKI.valid())
        futurePKI.wait();
}

void AsyncCardReader::clearPluginCredentials()
{
    auto active = activePlugin.load();
    if (active && session)
        active->clearCredentials(*session);
}

std::shared_ptr<LibreSCRS::Plugin::CardPlugin> AsyncCardReader::currentPlugin() const
{
    return activePlugin.load();
}

bool AsyncCardReader::hasPKI() const
{
    auto active = activePlugin.load();
    auto pki = pkiPlugin.load();
    return (active &&
            LibreSCRS::Plugin::hasCapability(active->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI)) ||
           (pki && LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI));
}

std::shared_ptr<LibreSCRS::Plugin::CardPlugin> AsyncCardReader::signingPlugin() const
{
    using Cap = LibreSCRS::Plugin::CardCapabilities;
    auto isSigningCapable = [](const std::shared_ptr<LibreSCRS::Plugin::CardPlugin>& p) {
        if (!p)
            return false;
        const auto caps = p->capabilities();
        return LibreSCRS::Plugin::hasCapability(caps, Cap::PKI) &&
               LibreSCRS::Plugin::hasCapability(caps, Cap::PinManagement);
    };
    // pkiPlugin is set when activePlugin doesn't itself have PKI (e.g.
    // an eMRTD identity plugin paired with a separately-loaded PKI
    // plugin); when set, it's always the right signing plugin.
    if (auto pki = pkiPlugin.load(); isSigningCapable(pki))
        return pki;
    // Common case: activePlugin (the one that read the card) is itself
    // PKI-capable — rs-eid is the canonical example, with all of
    // IdentityData + PKI + PinManagement.
    if (auto active = activePlugin.load(); isSigningCapable(active))
        return active;
    return {};
}

void AsyncCardReader::requestData()
{
    waitForPendingAsync();
    futureData = {};

    stopRequested = false;
    // Reset stop_source: a stop_source whose request_stop has fired stays
    // in the "stopped" state forever for any token derived from it. We
    // need a fresh source for the new request so cancellation only
    // applies to this run.
    stopSource = {};
    certsAlreadyQueued = false;
    emit readingStarted();

    QPointer<AsyncCardReader> self = this;
    futureData = std::async(std::launch::async, [this, self]() {
        if (!session) {
            QMetaObject::invokeMethod(
                self,
                [this, self]() {
                    if (!self)
                        return;
                    emit errorOccurred(
                        qtTrId("lc-error-no-connection")); // i18n-audit: ignore D1, transient status-bar message —
                                                           // qtTrId evaluated at emit time on GUI thread, not retained
                    emit readingFinished();
                },
                Qt::QueuedConnection);
            return;
        }

        // Note: if a candidate emits groups via onGroup then returns non-Ok,
        // those groups are already queued to the GUI thread. The user may
        // briefly see partial UI from the wrong plugin. When the correct
        // plugin succeeds, cardDataReady replaces the widget. Acceptable
        // tradeoff for v1 (avoids double card read).
        for (const auto& candidate : candidates) {
            if (stopRequested)
                return;
            auto callback = [self](const std::string& cardType, const LibreSCRS::Plugin::CardFieldGroup& group) {
                if (!self)
                    return;
                auto ct = QString::fromStdString(cardType);
                QMetaObject::invokeMethod(
                    self,
                    [self, ct, group]() {
                        if (!self)
                            return;
                        emit self->cardGroupReady(ct, group);
                    },
                    Qt::QueuedConnection);
            };

            // LM ABI v6 (4.0): readCard returns ReadResult (noexcept). A non-Ok
            // status means "try the next candidate". A defensive catch-all
            // remains so a buggy plugin that still throws cannot bubble
            // through the Qt event loop. Initial sentinel value uses the
            // generic ErrorKeys::genericComm message; readCard() overwrites
            // on success.
            //
            // Plan A T7 step 7.4: pass our stop_token so the plugin's NVI
            // wrapper observes cancellation between APDU groups.
            LibreSCRS::Plugin::ReadResult rr =
                LibreSCRS::Plugin::ReadResult::communicationError(LibreSCRS::Auth::ErrorKeys::genericComm());
            try {
                rr = candidate->readCard(*session, callback, stopSource.token());
            } catch (...) {
                continue;
            }
            if (rr.status != LibreSCRS::Plugin::ReadResult::Status::Ok || !rr.data.has_value())
                continue;
            auto data = std::move(*rr.data);
            QMetaObject::invokeMethod(
                self,
                [this, self, data = std::move(data), candidate]() {
                    if (!self)
                        return;
                    activePlugin.store(candidate);
                    pkiPlugin.store({});
                    if (!LibreSCRS::Plugin::hasCapability(candidate->capabilities(),
                                                          LibreSCRS::Plugin::CardCapabilities::PKI)) {
                        for (const auto& c : candidates) {
                            if (c != candidate && LibreSCRS::Plugin::hasCapability(
                                                      c->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI)) {
                                pkiPlugin.store(c);
                                break;
                            }
                        }
                    }
                    emit cardDataReady(data);
                    emit readingFinished();
                },
                Qt::QueuedConnection);
            return;
        }

        // All candidates failed
        QMetaObject::invokeMethod(
            self,
            [this, self]() {
                if (!self)
                    return;
                emit errorOccurred(qtTrId("lc-error-no-plugin"));
                emit readingFinished();
            },
            Qt::QueuedConnection);
    });
}

void AsyncCardReader::requestCertificates()
{
    // requestDataWithCredentials already read certs inline and queued
    // certificatesReady — skip redundant read.
    if (certsAlreadyQueued)
        return;

    auto pkiSnapshot = pkiPlugin.load();
    auto activeSnapshot = activePlugin.load();
    auto pki = pkiSnapshot ? pkiSnapshot : activeSnapshot;
    if (!pki || !LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI))
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki]() {
        if (stopRequested)
            return;
        try {
            // Read token info first
            auto tokenInfo = pki->readTokenInfo(*session);
            if (!tokenInfo.fields.empty()) {
                QMetaObject::invokeMethod(
                    self,
                    [self, tokenInfo = std::move(tokenInfo)]() {
                        if (!self)
                            return;
                        emit self->tokenInfoReady(tokenInfo);
                    },
                    Qt::QueuedConnection);
            }

            if (stopRequested)
                return;

            auto certs = pki->readCertificates(*session);
            QMetaObject::invokeMethod(
                self,
                [self, certs = std::move(certs)]() {
                    if (!self)
                        return;
                    emit self->certificatesReady(certs);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestPINTriesLeft(uint8_t pinReference)
{
    auto pkiSnapshot = pkiPlugin.load();
    auto activeSnapshot = activePlugin.load();
    auto pki = pkiSnapshot ? pkiSnapshot : activeSnapshot;
    if (!pki || !LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI))
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinReference]() {
        if (stopRequested)
            return;
        try {
            auto pins = pki->getPINList(*session);
            int tries = -1;
            for (const auto& p : pins) {
                if (p.reference == pinReference) {
                    tries = p.retriesLeft.value_or(-1);
                    break;
                }
            }
            if (tries == -1 && pins.empty()) {
                // Fallback to single-PIN API (optional<int>; nullopt → "unknown", rendered as -1).
                tries = pki->getPINTriesLeft(*session).value_or(-1);
            }
            QMetaObject::invokeMethod(
                self,
                [self, tries]() {
                    if (!self)
                        return;
                    emit self->pinStatusReady(tries, tries == 0);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestPINList()
{
    auto pkiSnapshot = pkiPlugin.load();
    auto activeSnapshot = activePlugin.load();
    auto pki = pkiSnapshot ? pkiSnapshot : activeSnapshot;
    if (!pki || !LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI))
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki]() {
        if (stopRequested)
            return;
        try {
            auto pins = pki->getPINList(*session);
            QMetaObject::invokeMethod(
                self,
                [self, pins = std::move(pins)]() {
                    if (!self)
                        return;
                    emit self->pinListReady(pins);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestChangePIN(uint8_t pinReference, const LibreSCRS::Secure::String& oldPin,
                                       const LibreSCRS::Secure::String& newPin)
{
    auto pkiSnapshot = pkiPlugin.load();
    auto activeSnapshot = activePlugin.load();
    auto pki = pkiSnapshot ? pkiSnapshot : activeSnapshot;
    if (!pki || !LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI))
        return;

    waitForPendingAsync();

    // PIN already lives in Secure::String all the way from ChangePinDlg
    // (PIN never enters QString-shared storage past the
    // QLineEdit textbox). Copy into the async lambda by value; each copy
    // is independently cleansed when its enclosing Secure::String dies.
    LibreSCRS::Secure::String oldPinCopy = oldPin;
    LibreSCRS::Secure::String newPinCopy = newPin;

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinReference, oldPinSecure = std::move(oldPinCopy),
                                                newPinSecure = std::move(newPinCopy)]() mutable {
        if (stopRequested)
            return;
        try {
            LibreSCRS::Plugin::PINResult result;
            {
                // ABI v6 changePIN takes Secure::String old/new directly.
                // pinLabel follows the "pin_<reference>" convention
                // understood by plugins that key off label (pkcs15-plugin);
                // plugins that key off a single applet PIN (opensc, cardedge)
                // ignore the label.
                const std::string pinLabel = "pin_" + std::to_string(pinReference);
                result = pki->changePIN(*session, pinLabel, oldPinSecure, newPinSecure);
            }

            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    const bool ok = result.ok();
                    emit self->pinChangeResult(ok, result.retriesLeft.value_or(-1),
                                               ok ? QString() : qtTrId("lc-changepin-failed"));
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->pinChangeResult(false, -1, msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestDataWithCredentials(EmrtdCredentials credentials)
{
    auto active = activePlugin.load();
    if (!active)
        return;

    waitForPendingAsync();

    stopRequested = false;
    stopSource = {};

    // Fan out the typed credential carrier into the plugin's keyed
    // setCredentials surface. Each Secure::String is moved so the copy lives
    // exactly as long as the plugin's stored credential cache; the local
    // carrier's fields are emptied as a side effect. No QString-backed
    // intermediate exists at any point on this path.
    if (credentials.mode == EmrtdCredentials::Mode::CAN) {
        if (!credentials.can.empty())
            active->setCredentials(*session, std::string_view{"can"}, credentials.can);
    } else {
        if (!credentials.mrzDocNumber.empty())
            active->setCredentials(*session, std::string_view{"mrz_doc_number"}, credentials.mrzDocNumber);
        if (!credentials.mrzDob.empty())
            active->setCredentials(*session, std::string_view{"mrz_dob"}, credentials.mrzDob);
        if (!credentials.mrzExpiry.empty())
            active->setCredentials(*session, std::string_view{"mrz_expiry"}, credentials.mrzExpiry);
    }

    futureData = {};

    emit readingStarted();

    QPointer<AsyncCardReader> self2 = this;
    futureData = std::async(std::launch::async, [this, self2, active]() {
        if (stopRequested)
            return;
        try {
            auto callback = [self2](const std::string& cardType, const LibreSCRS::Plugin::CardFieldGroup& group) {
                if (!self2)
                    return;
                auto ct = QString::fromStdString(cardType);
                QMetaObject::invokeMethod(
                    self2,
                    [self2, ct, group]() {
                        if (!self2)
                            return;
                        emit self2->cardGroupReady(ct, group);
                    },
                    Qt::QueuedConnection);
            };

            // LM ABI v6 (4.0): readCard returns ReadResult. Translate a non-Ok
            // outcome into a user-facing error on the GUI thread — the
            // authentication-phase (requestDataWithCredentials) is the
            // second attempt, so there is no further fallback.
            //
            // Plan A T7 step 7.4: pass stop_token so the plugin's NVI
            // wrapper observes cancellation between APDU groups.
            LibreSCRS::Plugin::ReadResult rr = active->readCard(*session, callback, stopSource.token());
            if (rr.status != LibreSCRS::Plugin::ReadResult::Status::Ok || !rr.data.has_value()) {
                std::string diag = rr.diagnosticDetail.value_or(std::string{"readCard failed"});
                QMetaObject::invokeMethod(
                    self2,
                    [this, self2, msg = QString::fromStdString(diag)]() {
                        if (!self2)
                            return;
                        emit errorOccurred(msg);
                        emit readingFinished();
                    },
                    Qt::QueuedConnection);
                return;
            }
            auto data = std::move(*rr.data);

            // PKI fallback — SM filter stays active (installed by eMRTD plugin
            // after PACE). Re-probe for PKI plugins that require authentication
            // (e.g., PKCS#15 on contactless needs PACE before AID SELECT).
            //
            // pkiPlugin / activePlugin are atomic<shared_ptr> so any thread
            // may load them safely; we still keep the member-write step in
            // the GUI-thread queued lambda below for ordering with the UI
            // signals it emits (cardDataReady / tokenInfoReady).
            std::shared_ptr<LibreSCRS::Plugin::CardPlugin> probedPki;
            const auto initialPkiPlugin = pkiPlugin.load();
            if (!initialPkiPlugin &&
                !LibreSCRS::Plugin::hasCapability(active->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI)) {
                const auto& atrBytes = session->atr();
                for (const auto& p : allPlugins) {
                    if (p == active)
                        continue;
                    try {
                        if (LibreSCRS::Plugin::hasCapability(p->capabilities(),
                                                             LibreSCRS::Plugin::CardCapabilities::PKI) &&
                            p->canHandleConnection(atrBytes, *session)) {
                            probedPki = p;
                            break;
                        }
                    } catch (...) {
                    }
                }
            }

            std::vector<LibreSCRS::Plugin::CertificateData> certs;
            LibreSCRS::Plugin::CardFieldGroup tokenInfo;
            // pki is the resolved plugin used for token-info and certificate
            // reads on this worker thread. Its identity is one of:
            //   - probedPki (just-discovered fallback PKI plugin)
            //   - initialPkiPlugin (pre-existing pkiPlugin from earlier)
            //   - active (when the active plugin is itself PKI-capable)
            std::shared_ptr<LibreSCRS::Plugin::CardPlugin> pki = probedPki ? probedPki : initialPkiPlugin;
            if (!pki && active &&
                LibreSCRS::Plugin::hasCapability(active->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI)) {
                pki = active;
            }
            bool certReadFailed = false;
            if (pki &&
                LibreSCRS::Plugin::hasCapability(pki->capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI)) {
                try {
                    tokenInfo = pki->readTokenInfo(*session);
                } catch (...) {
                }
                try {
                    certs = pki->readCertificates(*session);
                } catch (...) {
                    // Certificate reading failed (e.g. CL without PACE/SM).
                    // Signal the GUI-thread lambda below to clear pkiPlugin
                    // iff it still equals the pki we used here — avoids
                    // showing an empty token section and spurious errors.
                    certReadFailed = true;
                }
            }

            QMetaObject::invokeMethod(
                self2,
                [this, self2, data = std::move(data), certs = std::move(certs), tokenInfo = std::move(tokenInfo),
                 probedPki, pki, certReadFailed]() {
                    if (!self2)
                        return;
                    // pkiPlugin is std::atomic<std::shared_ptr<...>>; reads
                    // and writes here are race-free against any worker
                    // thread that may be inspecting it.
                    if (probedPki && !pkiPlugin.load())
                        pkiPlugin.store(probedPki);
                    if (certReadFailed && pkiPlugin.load() == pki)
                        pkiPlugin.store({});
                    certsAlreadyQueued = !certs.empty();
                    emit cardDataReady(data);
                    emit readingFinished();
                    if (!tokenInfo.fields.empty())
                        emit tokenInfoReady(tokenInfo);
                    if (!certs.empty()) {
                        QMetaObject::invokeMethod(
                            self2,
                            [this, self2, certs]() {
                                if (!self2)
                                    return;
                                emit certificatesReady(certs);
                            },
                            Qt::QueuedConnection);
                    }
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self2,
                [this, self2, msg = QString::fromStdString(e.what())]() {
                    if (!self2)
                        return;
                    emit errorOccurred(msg);
                    emit readingFinished();
                },
                Qt::QueuedConnection);
        }
    });
}
