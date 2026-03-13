// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me
// Originally derived from Ludovic Rousseau's pcsc_scan.c, since fully refactored

#include "smartcardscanner.h"
#include "smartcardevent.h"
#include "utils/libreceliklog.h"
#include "ipcscscanprovider.h"
#include "pcscscanprovider.h"
#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif
#include <QThread>
#include <stdexcept>

static constexpr DWORD SCAN_TIMEOUT = 3600 * 1000; // 1 hour
static constexpr const char* PNP_READER = "\\\\?PnP?\\Notification";

SmartCardScanner::SmartCardScanner(QObject* parent) : SmartCardScanner(std::make_unique<PCSCScanProvider>(), parent) {}

SmartCardScanner::SmartCardScanner(std::unique_ptr<IPCSCScanProvider> provider, QObject* parent)
    : QObject{parent}, pcsc(std::move(provider))
{}

SmartCardScanner::~SmartCardScanner() = default;

void SmartCardScanner::requestStop()
{
    qCDebug(librecSCRSCard) << "SmartCardWorker: Requesting stop!";
    pcsc->cancel(hContext);
}

void SmartCardScanner::establishContext()
{
    LONG rv = pcsc->establishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext);
    if (rv != SCARD_S_SUCCESS) {
        qCDebug(librecSCRSCard) << "SCardEstablishContext failed:" << rv;
        throw std::runtime_error("Cannot establish context in SmartCardScanner");
    }
}

bool SmartCardScanner::checkPnPSupport()
{
    SCARD_READERSTATE state{};
    state.szReader = PNP_READER;
    state.dwCurrentState = SCARD_STATE_UNAWARE;

    pcsc->getStatusChange(hContext, 0, &state, 1);
    if (state.dwEventState & SCARD_STATE_UNKNOWN) {
        qCDebug(librecSCRSCard) << "SmartCardScanner: PNP not available";
        return false;
    }
    qCDebug(librecSCRSCard) << "SmartCardScanner: Using PNP";
    return true;
}

std::vector<std::string> SmartCardScanner::enumerateReaders()
{
    DWORD dwReaders = 0;
    LONG rv = pcsc->listReaders(hContext, nullptr, nullptr, &dwReaders);

    if (rv == SCARD_E_NO_READERS_AVAILABLE || dwReaders == 0) {
        return {};
    }

    if (rv != SCARD_S_SUCCESS) {
        qCDebug(librecSCRSCard) << "SCardListReaders failed:" << rv;
        pcsc->releaseContext(hContext);
        rv = pcsc->establishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext);
        if (rv != SCARD_S_SUCCESS) {
            throw std::runtime_error("Cannot re-establish context in SmartCardScanner");
        }
        return {};
    }

    std::vector<char> buffer(dwReaders);
    buffer[0] = '\0';
    rv = pcsc->listReaders(hContext, nullptr, buffer.data(), &dwReaders);

    if (rv == SCARD_E_NO_READERS_AVAILABLE) {
        return {};
    }
    if (rv != SCARD_S_SUCCESS) {
        qCDebug(librecSCRSCard) << "SCardListReaders (second call) failed:" << rv;
        return {};
    }

    std::vector<std::string> readers;
    const char* ptr = buffer.data();
    while (*ptr != '\0') {
        readers.emplace_back(ptr);
        ptr += readers.back().size() + 1;
    }
    return readers;
}

void SmartCardScanner::waitForFirstReader(bool pnp)
{
    if (pnp) {
        SCARD_READERSTATE state{};
        state.szReader = PNP_READER;
        state.dwCurrentState = SCARD_STATE_UNAWARE;

        LONG rv;
        do {
            rv = pcsc->getStatusChange(hContext, SCAN_TIMEOUT, &state, 1);
        } while (rv == SCARD_E_TIMEOUT && !QThread::currentThread()->isInterruptionRequested());

        if (rv != SCARD_S_SUCCESS) {
            qCDebug(librecSCRSCard) << "SCardGetStatusChange (PnP wait) failed:" << rv;
            pcsc->releaseContext(hContext);
            LONG rv2 = pcsc->establishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext);
            if (rv2 != SCARD_S_SUCCESS) {
                throw std::runtime_error("Cannot re-establish context in SmartCardScanner");
            }
        }
    } else {
        DWORD dwReaders = 0;
        DWORD dwReadersOld = 0;
        pcsc->listReaders(hContext, nullptr, nullptr, &dwReadersOld);
        dwReaders = dwReadersOld;

        LONG rv = SCARD_S_SUCCESS;
        while ((rv == SCARD_S_SUCCESS) && (dwReaders == dwReadersOld)) {
            rv = pcsc->listReaders(hContext, nullptr, nullptr, &dwReaders);
            if (rv == SCARD_E_NO_READERS_AVAILABLE) {
                rv = SCARD_S_SUCCESS;
            }
            QThread::sleep(1);
            if (QThread::currentThread()->isInterruptionRequested()) {
                return;
            }
        }
    }
}

bool SmartCardScanner::processEvents(std::vector<SCARD_READERSTATE>& states, int readerCount, bool pnp)
{
    int totalStates = pnp ? readerCount + 1 : readerCount;

    // Non-blocking probe to capture initial card state.
    // On macOS, SCardGetStatusChange with SCARD_STATE_UNAWARE and a long timeout
    // can block indefinitely on certain readers (e.g. HID OMNIKEY 5422).
    LONG rv = pcsc->getStatusChange(hContext, 0, states.data(), totalStates);

    while ((rv == SCARD_S_SUCCESS) || (rv == SCARD_E_TIMEOUT)) {
        if (pnp) {
            if (states[readerCount].dwEventState & SCARD_STATE_CHANGED) {
                return true; // re-enumerate
            }
        } else {
            DWORD dwReaders = 0;
            DWORD dwReadersOld = 0;
            // Check current reader count vs what we had
            for (int i = 0; i < readerCount; i++) {
                dwReadersOld += strlen(states[i].szReader) + 1;
            }
            dwReadersOld += 1; // trailing null
            if ((pcsc->listReaders(hContext, nullptr, nullptr, &dwReaders) == SCARD_S_SUCCESS) &&
                (dwReaders != dwReadersOld)) {
                return true; // re-enumerate
            }
        }

        if (rv != SCARD_E_TIMEOUT) {
            time_t t = time(nullptr);
            qCDebug(librecSCRSCard) << "SmartCardScanner: Event" << rv << "Time:" << ctime(&t);
        }

        bool needReEnumeration = false;
        for (int i = 0; i < readerCount; i++) {
#if defined(__APPLE__) || defined(WIN32)
            if (states[i].dwCurrentState == states[i].dwEventState) {
                continue;
            }
#endif
            DWORD dwPrevState = states[i].dwCurrentState;
            if (states[i].dwEventState & SCARD_STATE_CHANGED) {
                states[i].dwCurrentState = states[i].dwEventState & ~SCARD_STATE_CHANGED;
            } else {
                continue;
            }

            qCDebug(librecSCRSCard) << "SmartCardScanner: Reader" << i << "State:" << states[i].szReader;
            qCDebug(librecSCRSCard) << "SmartCardScanner: Event number:" << (states[i].dwEventState >> 4);
            qCDebug(librecSCRSCard) << "SmartCardScanner: Card state:";

            if (states[i].dwEventState & SCARD_STATE_IGNORE) {
                qCDebug(librecSCRSCard) << "SmartCardScanner:     Ignore this reader,";
            }

            if (states[i].dwEventState & SCARD_STATE_UNKNOWN) {
                qCDebug(librecSCRSCard) << "SmartCardScanner:     Unknown";
                emit smartCardEventOccured({states[i].szReader, SmartCardEvent::EventType::CardRemoved});
                needReEnumeration = true;
                break;
            }

            SmartCardEvent::EventType eventType = SmartCardEvent::Unknown;
            if (states[i].dwEventState & SCARD_STATE_UNAVAILABLE) {
                qCDebug(librecSCRSCard) << " SmartCardScanner:    Status unavailable";
            }

            if (states[i].dwEventState & SCARD_STATE_EMPTY) {
                qCDebug(librecSCRSCard) << "SmartCardScanner:     Card removed";
                eventType = SmartCardEvent::EventType::CardRemoved;
            }

            if (states[i].dwEventState & SCARD_STATE_PRESENT) {
                if (states[i].dwEventState & SCARD_STATE_EXCLUSIVE) {
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Exclusive Mode";
                    continue;
                } else if (states[i].dwEventState & SCARD_STATE_MUTE) {
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Unresponsive card";
                    continue;
                } else if (dwPrevState & SCARD_STATE_PRESENT) {
                    if ((dwPrevState >> 16) == (states[i].dwEventState >> 16)) {
                        qCDebug(librecSCRSCard) << "SmartCardScanner:     Card state updated (already present)";
                        continue;
                    }
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Card swapped (new card detected)";
                    emit smartCardEventOccured({states[i].szReader, SmartCardEvent::EventType::CardRemoved});
                    eventType = SmartCardEvent::EventType::CardInserted;
                } else {
                    qCDebug(librecSCRSCard) << "SmartCardScanner:     Card inserted";
                    eventType = SmartCardEvent::EventType::CardInserted;
                }
            }

            if (states[i].dwEventState & SCARD_STATE_ATRMATCH) {
                qCDebug(librecSCRSCard) << "SmartCardScanner:     ATR matches card";
            }

            if (states[i].dwEventState & SCARD_STATE_UNPOWERED) {
                qCDebug(librecSCRSCard) << "SmartCardScanner:     Unpowered card";
            }

            emit smartCardEventOccured({states[i].szReader, eventType});
        }

        if (needReEnumeration) {
            return true;
        }

        if (QThread::currentThread()->isInterruptionRequested())
            break;

        rv = pcsc->getStatusChange(hContext, SCAN_TIMEOUT, states.data(), totalStates);
    }

    // Post-loop error handling
    if (rv == SCARD_E_NO_SERVICE) {
        qCDebug(librecSCRSCard) << "SmartCardScanner: SCARD_E_NO_SERVICE";

        // Emit CardRemoved for all active readers
        for (int i = 0; i < readerCount; i++) {
            emit smartCardEventOccured({states[i].szReader, SmartCardEvent::EventType::CardRemoved});
        }

        // Re-establish context
        pcsc->releaseContext(hContext);
        LONG rv2 = pcsc->establishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext);
        if (rv2 != SCARD_S_SUCCESS) {
            throw std::runtime_error("Cannot re-establish context in SmartCardScanner");
        }
        return true;
    }

    if (rv == SCARD_E_UNKNOWN_READER) {
        return true; // re-enumerate
    }

    if (rv == SCARD_E_CANCELLED) {
        qCDebug(librecSCRSCard) << "SmartCardScanner: SCARD_E_CANCELLED";
        return false; // stop
    }

    // Other error — re-establish context
    if (rv != SCARD_S_SUCCESS) {
        qCDebug(librecSCRSCard) << "SCardGetStatusChange failed:" << rv;
        pcsc->releaseContext(hContext);
        LONG rv2 = pcsc->establishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hContext);
        if (rv2 != SCARD_S_SUCCESS) {
            throw std::runtime_error("Cannot re-establish context in SmartCardScanner");
        }
    }
    return true;
}

void SmartCardScanner::doWork()
{
    establishContext();
    bool pnp = checkPnPSupport();

    while (!QThread::currentThread()->isInterruptionRequested()) {
        qCDebug(librecSCRSCard) << "SmartCardScanner: Scanning present readers...";
        auto readers = enumerateReaders();

        if (readers.empty()) {
            qCDebug(librecSCRSCard) << "Waiting for the first reader...";
            emit smartCardReaderEnumerationChanged({});
            waitForFirstReader(pnp);
            qCDebug(librecSCRSCard) << "Found one. Scanning again...";
            continue;
        }

        // Build reader state array, preserving known state for surviving readers
        int readerCount = static_cast<int>(readers.size());
        int stateCount = pnp ? readerCount + 1 : readerCount;
        std::vector<SCARD_READERSTATE> states(stateCount, SCARD_READERSTATE{});

        QStringList scrNames;
        for (int i = 0; i < readerCount; i++) {
            scrNames << QString::fromStdString(readers[i]);
            states[i].szReader = readers[i].c_str();
            states[i].cbAtr = sizeof(states[i].rgbAtr);

            auto it = previousReaderStates.find(readers[i]);
            if (it != previousReaderStates.end()) {
                // Carry forward the last known state so the card isn't re-detected
                states[i].dwCurrentState = it->second;
            } else {
                states[i].dwCurrentState = SCARD_STATE_UNAWARE;
            }
        }

        if (pnp) {
            states[readerCount].szReader = PNP_READER;
            states[readerCount].dwCurrentState = SCARD_STATE_UNAWARE;
        }

        emit smartCardReaderEnumerationChanged(scrNames);

        if (!processEvents(states, readerCount, pnp)) {
            break; // cancelled
        }

        // Save current reader states for the next enumeration cycle
        previousReaderStates.clear();
        for (int i = 0; i < readerCount; i++) {
            previousReaderStates[readers[i]] = states[i].dwCurrentState;
        }
    }

    qCDebug(librecSCRSCard) << "SmartCardScanner: QThread::currentThread interrupted!";
    pcsc->releaseContext(hContext);
}
