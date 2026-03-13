// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDSCANNER_TEST_H
#define SMARTCARDSCANNER_TEST_H

#include "smartcard/ipcscscanprovider.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Shared counters that survive mock destruction.
struct MockCounters
{
    std::atomic<int> establishContextCount{0};
    std::atomic<int> releaseContextCount{0};
    std::atomic<int> listReadersCount{0};
    std::atomic<int> getStatusChangeCount{0};
    std::atomic<int> cancelCount{0};
};

// Scripted mock for IPCSCScanProvider.
// Tests push "actions" that define what each PC/SC call returns and
// how it mutates reader states.  The mock replays them in order.
class MockPCSCScanProvider : public IPCSCScanProvider
{
public:
    explicit MockPCSCScanProvider(std::shared_ptr<MockCounters> c) : counters(std::move(c)) {}

    // --- Scripting types ---

    struct StatusChangeAction
    {
        LONG returnValue = SCARD_S_SUCCESS;
        // If non-empty, apply these event states to the reader states array
        std::vector<DWORD> eventStates;
        // If true, block until unblocked by cancel() or unblock()
        bool blocking = false;
        // If set, update the reader list after this action is consumed
        // (simulates a reader being plugged/unplugged between events)
        std::optional<std::vector<std::string>> newReaders;
    };

    // --- Configuration ---

    // Readers returned by listReaders
    void setReaders(std::vector<std::string> names)
    {
        std::lock_guard<std::mutex> lock(mtx);
        readerNames = std::move(names);
    }

    void setListReadersReturn(LONG rv)
    {
        std::lock_guard<std::mutex> lock(mtx);
        listReadersRv = rv;
    }

    // Queue a getStatusChange response
    void pushStatusChange(StatusChangeAction action)
    {
        std::lock_guard<std::mutex> lock(mtx);
        statusChangeQueue.push_back(std::move(action));
    }

    // --- IPCSCScanProvider implementation ---

    LONG establishContext(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT phContext) override
    {
        *phContext = 42; // dummy context
        counters->establishContextCount++;
        return SCARD_S_SUCCESS;
    }

    LONG releaseContext(SCARDCONTEXT) override
    {
        counters->releaseContextCount++;
        return SCARD_S_SUCCESS;
    }

    LONG listReaders(SCARDCONTEXT, LPCSTR, LPSTR mszReaders, LPDWORD pcchReaders) override
    {
        std::lock_guard<std::mutex> lock(mtx);
        counters->listReadersCount++;

        if (listReadersRv != SCARD_S_SUCCESS) {
            return listReadersRv;
        }

        if (readerNames.empty()) {
            *pcchReaders = 0;
            return SCARD_E_NO_READERS_AVAILABLE;
        }

        // Calculate needed size
        DWORD needed = 1; // trailing null
        for (const auto& name : readerNames) {
            needed += name.size() + 1;
        }

        if (mszReaders == nullptr) {
            *pcchReaders = needed;
            return SCARD_S_SUCCESS;
        }

        // Fill multi-string buffer
        char* ptr = mszReaders;
        for (const auto& name : readerNames) {
            std::memcpy(ptr, name.c_str(), name.size() + 1);
            ptr += name.size() + 1;
        }
        *ptr = '\0';
        *pcchReaders = needed;
        return SCARD_S_SUCCESS;
    }

    LONG getStatusChange(SCARDCONTEXT, DWORD, SCARD_READERSTATE* rgReaderStates, DWORD cReaders) override
    {
        StatusChangeAction action;
        {
            std::lock_guard<std::mutex> lock(mtx);
            counters->getStatusChangeCount++;

            if (cancelled) {
                cancelled = false;
                return SCARD_E_CANCELLED;
            }

            if (statusChangeQueue.empty()) {
                // Default: return cancelled to stop the loop
                return SCARD_E_CANCELLED;
            }

            action = statusChangeQueue.front();
            statusChangeQueue.erase(statusChangeQueue.begin());

            // Update reader list if scheduled
            if (action.newReaders) {
                readerNames = std::move(*action.newReaders);
            }
        }

        if (action.blocking) {
            std::unique_lock<std::mutex> lock(blockMtx);
            blocked = true;
            blockCv.wait(lock, [this] { return !blocked; });

            std::lock_guard<std::mutex> lock2(mtx);
            if (cancelled) {
                cancelled = false;
                return SCARD_E_CANCELLED;
            }
        }

        // Apply event states
        for (DWORD i = 0; i < cReaders && i < action.eventStates.size(); i++) {
            rgReaderStates[i].dwEventState = action.eventStates[i];
        }

        return action.returnValue;
    }

    LONG cancel(SCARDCONTEXT) override
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            cancelled = true;
            counters->cancelCount++;
        }
        // Also unblock if waiting
        {
            std::lock_guard<std::mutex> lock(blockMtx);
            blocked = false;
            blockCv.notify_all();
        }
        return SCARD_S_SUCCESS;
    }

private:
    std::shared_ptr<MockCounters> counters;

    std::mutex mtx;
    std::vector<std::string> readerNames;
    LONG listReadersRv = SCARD_S_SUCCESS;
    std::vector<StatusChangeAction> statusChangeQueue;

    std::mutex blockMtx;
    std::condition_variable blockCv;
    bool blocked = false;
    bool cancelled = false;
};

#endif // SMARTCARDSCANNER_TEST_H
