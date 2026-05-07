// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "document/emrtd/emrtdcredentials.h"
#include "utils/atomic_shared_ptr.h"

#include <LibreSCRS/CancelToken.h>
#include <LibreSCRS/Plugin/CardPlugin.h>
#include <LibreSCRS/Secure/String.h>
#include <LibreSCRS/SmartCard/CardSession.h>

#include <QObject>

#include <atomic>
#include <future>
#include <memory>
#include <vector>

class AsyncCardReader : public QObject
{
    Q_OBJECT
public:
    // candidates: middleware plugins sorted by priority (shared ownership).
    // allPlugins: full registry snapshot for PKI fallback (shared ownership).
    // session:    shared ownership of the active CardSession; the AsyncCardReader
    //             retains its own shared_ptr for the lifetime of async work.
    //
    // Plugin ownership is std::shared_ptr<CardPlugin> end-to-end —
    // CardPluginService hands out shared_ptrs, AsyncCardReader stores them,
    // and SigningService::sign consumes them directly with no
    // no-op-deleter aliasing hack at any seam.
    AsyncCardReader(std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> candidates,
                    std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> allPlugins,
                    std::shared_ptr<LibreSCRS::SmartCard::CardSession> session, QObject* parent = nullptr);
    ~AsyncCardReader();

    void requestData();
    void requestCertificates();
    void requestPINTriesLeft(uint8_t pinReference);
    void requestPINList();
    void requestChangePIN(uint8_t pinReference, const LibreSCRS::Secure::String& oldPin,
                          const LibreSCRS::Secure::String& newPin);
    /// @brief eMRTD authentication entry point. CAN/MRZ travel through
    ///        @ref EmrtdCredentials backed by Secure::String — no QString
    ///        CoW copies of the secrets exist past this call.
    /// @note  The carrier is taken by value so the queued connection may
    ///        copy it during the signal/slot hop without leaking plaintext.
    void requestDataWithCredentials(EmrtdCredentials credentials);

    // Signal async threads to stop and block until they finish.
    // Safe to call multiple times. Called automatically by the destructor.
    void cancel();

    // Signal async threads to stop without blocking. The caller must ensure
    // the object stays alive until workers finish (e.g. via background cleanup).
    void initiateCancel();

    // Block until async workers complete. Used by background cleanup threads.
    void waitForPendingAsync();

    std::shared_ptr<LibreSCRS::Plugin::CardPlugin> currentPlugin() const;
    bool hasPKI() const;
    void clearPluginCredentials();

signals:
    void cardGroupReady(const QString& cardType, const LibreSCRS::Plugin::CardFieldGroup& group);
    void cardDataReady(const LibreSCRS::Plugin::CardData& data);
    void certificatesReady(const std::vector<LibreSCRS::Plugin::CertificateData>& certs);
    void tokenInfoReady(const LibreSCRS::Plugin::CardFieldGroup& tokenInfo);
    void pinStatusReady(int retriesLeft, bool blocked);
    void pinChangeResult(bool success, int retriesLeft, const QString& errorMessage);
    void pinListReady(const std::vector<LibreSCRS::Plugin::PinStatusEntry>& pins);
    void errorOccurred(const QString& message);
    void readingStarted();
    void readingFinished();

private:
    std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> candidates;
    std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> allPlugins;

    // Plugin shared_ptrs are safe to capture in async lambdas: shared ownership
    // keeps the underlying plugin object (and, via the registry's custom
    // deleter, the dlopen handle) alive for as long as any worker retains a
    // reference — no registry-lifetime dependency remains.
    //
    // Wrapped in AtomicSharedPtr (mutex-guarded shared_ptr with the same
    // .load()/.store() API as C++20 std::atomic<std::shared_ptr<T>>) so
    // worker threads and the GUI thread can read/write the active and PKI
    // plugin pointers concurrently without tearing the control-block-pointer
    // pair. Reads snapshot via .load() at the top of each scope; writes go
    // through .store(). Replaces an earlier discipline-based scheme that
    // documented "GUI-thread-only writes via Qt::QueuedConnection"; the
    // type now enforces that race-free contract instead of relying on
    // convention. See utils/atomic_shared_ptr.h for the libc++ portability
    // rationale (P0718R2 not yet implemented).
    librecelik::detail::AtomicSharedPtr<LibreSCRS::Plugin::CardPlugin> activePlugin;
    librecelik::detail::AtomicSharedPtr<LibreSCRS::Plugin::CardPlugin> pkiPlugin;
    std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
    /// @brief Cooperative cancellation flag observed at every async-step
    ///        boundary inside this class.
    ///
    /// Retained alongside @c stopSource because individual call sites
    /// (between APDU groups, between PKI sub-operations) still query a
    /// plain atomic for clarity. The two are kept in lockstep: every
    /// path that flips @c stopRequested also calls @c stopSource.requestCancel().
    std::atomic<bool> stopRequested{false};
    /// @brief LibreSCRS cooperative-cancellation source plumbed into LM 4.0
    ///        readCard / sign NVI wrappers. The associated token is consumed
    ///        by the plugin between APDU groups so cancellation can
    ///        short-circuit the loop without relying on SCardCancel (which
    ///        the PC/SC spec does not guarantee against SCardTransmit on any
    ///        platform).
    LibreSCRS::CancelSource stopSource;
    bool certsAlreadyQueued = false;
    std::future<void> futureData;
    std::future<void> futurePKI;
};
