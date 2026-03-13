// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "smartcardscanner_test.h"
#include "smartcard/smartcardscanner.h"
#include "smartcard/smartcardevent.h"
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

// Helper: run SmartCardScanner::doWork() in a QThread, process events, then clean up.
// The mock must be configured to eventually return SCARD_E_CANCELLED or the thread
// must be interrupted, otherwise this will hang.
class ScannerTestFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QCoreApplication::instance()) {
            int argc = 0;
            app = new QCoreApplication(argc, nullptr);
        }
        counters = std::make_shared<MockCounters>();
    }

    void TearDown() override
    {
        delete app;
        app = nullptr;
    }

    struct RunResult
    {
        QStringList lastReaderList;
        std::vector<SmartCardEvent> events;
    };

    RunResult runScanner(std::unique_ptr<MockPCSCScanProvider> mock, int maxWaitMs = 3000)
    {
        auto scanner = new SmartCardScanner(std::move(mock));
        auto thread = new QThread;
        scanner->moveToThread(thread);

        RunResult result;

        QObject::connect(thread, &QThread::started, scanner, &SmartCardScanner::doWork);
        QObject::connect(scanner, &SmartCardScanner::smartCardReaderEnumerationChanged,
                         [&result](QStringList names) { result.lastReaderList = names; });
        QObject::connect(scanner, &SmartCardScanner::smartCardEventOccured,
                         [&result](SmartCardEvent sce) { result.events.push_back(sce); });

        QObject::connect(thread, &QThread::finished, scanner, &QObject::deleteLater);
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

        thread->start();

        // Process events while waiting for thread to finish
        QElapsedTimer timer;
        timer.start();
        while (thread->isRunning() && timer.elapsed() < maxWaitMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(10);
        }

        if (thread->isRunning()) {
            scanner->requestStop();
            thread->requestInterruption();
            thread->quit();
            thread->wait(2000);
        }

        // Process remaining queued signals
        QCoreApplication::processEvents();

        return result;
    }

    std::shared_ptr<MockCounters> counters;

private:
    QCoreApplication* app = nullptr;
};

// --- Test: No readers available, then cancelled ---
TEST_F(ScannerTestFixture, NoReadersEmitsEmptyList)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);

    // PnP check: return UNKNOWN (no PnP)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});
    // waitForFirstReader will poll listReaders; no readers → timeout hit → force cancel
    // The mock's default empty statusChangeQueue returns SCARD_E_CANCELLED

    auto result = runScanner(std::move(mock));

    EXPECT_TRUE(result.lastReaderList.isEmpty());
    EXPECT_TRUE(result.events.empty());
    EXPECT_GE(counters->establishContextCount.load(), 1);
}

// --- Test: One reader, card inserted ---
TEST_F(ScannerTestFixture, CardInserted)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // processEvents: initial getStatusChange — card present on reader, PnP unchanged
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED, 0 /* PnP unchanged */}, false});

    // Next getStatusChange: cancelled (stop)
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    EXPECT_EQ(result.lastReaderList.size(), 1);
    EXPECT_EQ(result.lastReaderList[0], "Reader A");
    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[0].readerName, "Reader A");
}

// --- Test: One reader, card removed ---
TEST_F(ScannerTestFixture, CardRemoved)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // processEvents: card empty (removed)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED, 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardRemoved);
}

// --- Test: Card insert then remove sequence ---
TEST_F(ScannerTestFixture, CardInsertThenRemove)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card inserted
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED, 0}, false});

    // Card removed (event counter incremented in upper bits)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED | (2 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 2u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);
}

// --- Test: Card present + INUSE (Firefox holding connection), card was NOT previously present ---
TEST_F(ScannerTestFixture, CardPresentWithInuse)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card present + in use (first time seeing it)
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_INUSE | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
}

// --- Test: Card already present, INUSE flag toggled (Firefox connects) — no re-emission ---
TEST_F(ScannerTestFixture, InuseToggleNoReEmission)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Initial: card inserted (event counter = 1)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Second event: card still present, INUSE added, same event counter — should NOT re-emit
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_INUSE | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // Only the initial insert, no duplicate
    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
}

// --- Test: Card swap (present→present with different event counter) ---
TEST_F(ScannerTestFixture, CardSwap)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card inserted (event counter = 1)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Card swapped (event counter = 2, still present)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (2 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // Insert + (Remove for old + Insert for new) = 3 events
    ASSERT_EQ(result.events.size(), 3u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);
    EXPECT_EQ(result.events[2].eventType, SmartCardEvent::CardInserted);
}

// --- Test: Exclusive mode — card skipped ---
TEST_F(ScannerTestFixture, ExclusiveModeSkipped)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card in exclusive mode
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_EXCLUSIVE | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // No card events emitted (exclusive mode is skipped)
    EXPECT_TRUE(result.events.empty());
}

// --- Test: SCARD_STATE_UNKNOWN triggers re-enumeration with CardRemoved ---
TEST_F(ScannerTestFixture, UnknownStateTriggersReEnumeration)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Reader goes unknown (reader disconnected) → emits CardRemoved, then re-enumerates
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN | SCARD_STATE_CHANGED, 0}, false});

    // After re-enumeration with same readers, cancel
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // CardRemoved emitted for the unknown reader
    ASSERT_GE(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardRemoved);
}

// --- Test: SCARD_E_NO_SERVICE triggers cleanup and re-establish ---
TEST_F(ScannerTestFixture, NoServiceReEstablishesContext)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card present
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // PC/SC service goes down
    mock->pushStatusChange({SCARD_E_NO_SERVICE, {}, false});

    // After re-establish, scanner loops back — second enumeration then cancel
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // Should have: CardInserted, then CardRemoved (cleanup from NO_SERVICE)
    ASSERT_GE(result.events.size(), 2u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);

    // Context was re-established (initial + after NO_SERVICE)
    EXPECT_GE(counters->establishContextCount.load(), 2);
}

// --- Test: PnP reader count change triggers re-enumeration ---
TEST_F(ScannerTestFixture, PnPReaderChangeTriggersReEnumeration)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // processEvents: PnP slot (index 1) reports change
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {0 /* reader A unchanged */, SCARD_STATE_CHANGED /* PnP changed */}, false});

    // After re-enumeration: cancel
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // Reader list was emitted at least once
    EXPECT_FALSE(result.lastReaderList.isEmpty());
}

// --- Test: Multiple readers ---
TEST_F(ScannerTestFixture, MultipleReaders)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A", "Reader B"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card in reader B (index 1)
    mock->pushStatusChange(
        {SCARD_S_SUCCESS,
         {0 /* Reader A no change */, SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0 /* PnP */},
         false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    EXPECT_EQ(result.lastReaderList.size(), 2);
    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[0].readerName, "Reader B");
}

// --- Test: requestStop cancels blocking call ---
TEST_F(ScannerTestFixture, RequestStopCancels)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Blocking call — will wait until cancel() is called
    mock->pushStatusChange({SCARD_S_SUCCESS, {}, true});

    auto scanner = new SmartCardScanner(std::move(mock));
    auto thread = new QThread;
    scanner->moveToThread(thread);

    std::vector<SmartCardEvent> events;
    QObject::connect(thread, &QThread::started, scanner, &SmartCardScanner::doWork);
    QObject::connect(scanner, &SmartCardScanner::smartCardEventOccured,
                     [&events](SmartCardEvent sce) { events.push_back(sce); });
    QObject::connect(thread, &QThread::finished, scanner, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();

    // Let scanner start and block
    QThread::msleep(200);
    QCoreApplication::processEvents();

    // Request stop
    scanner->requestStop();
    thread->requestInterruption();
    thread->quit();
    thread->wait(2000);

    EXPECT_GE(counters->cancelCount.load(), 1);
}

// --- Test: Mute card is skipped ---
TEST_F(ScannerTestFixture, MuteCardSkipped)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card present but mute
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_MUTE | SCARD_STATE_CHANGED | (1 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    EXPECT_TRUE(result.events.empty());
}

// ========== Non-PnP path tests ==========

// --- Test: Non-PnP card inserted ---
TEST_F(ScannerTestFixture, NoPnP_CardInserted)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // processEvents (no PnP): card present
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[0].readerName, "Reader A");
}

// --- Test: Non-PnP card removed ---
TEST_F(ScannerTestFixture, NoPnP_CardRemoved)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // Card empty (removed)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardRemoved);
}

// --- Test: Non-PnP card insert then remove ---
TEST_F(ScannerTestFixture, NoPnP_CardInsertThenRemove)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // Card inserted
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED}, false});

    // Card removed
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED | (2 << 16)}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 2u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);
}

// --- Test: Non-PnP card swap ---
TEST_F(ScannerTestFixture, NoPnP_CardSwap)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // Card inserted (event counter = 1)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16)}, false});

    // Card swapped (event counter = 2, still present)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (2 << 16)}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 3u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);
    EXPECT_EQ(result.events[2].eventType, SmartCardEvent::CardInserted);
}

// --- Test: Non-PnP INUSE toggle on already-present card — no re-emission ---
TEST_F(ScannerTestFixture, NoPnP_InuseToggleNoReEmission)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // Card inserted (event counter = 1)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16)}, false});

    // INUSE toggled, same event counter
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_INUSE | SCARD_STATE_CHANGED | (1 << 16)}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_EQ(result.events.size(), 1u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
}

// --- Test: Non-PnP NO_SERVICE cleanup ---
TEST_F(ScannerTestFixture, NoPnP_NoServiceReEstablishesContext)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: UNKNOWN → no PnP
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    // Card present
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16)}, false});

    // NO_SERVICE
    mock->pushStatusChange({SCARD_E_NO_SERVICE, {}, false});

    // After re-establish, cancel
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    ASSERT_GE(result.events.size(), 2u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardRemoved);
    EXPECT_GE(counters->establishContextCount.load(), 2);
}

// ========== State preservation across re-enumeration ==========

// --- Test: Two readers with cards, one reader unplugged — surviving card NOT re-read ---
TEST_F(ScannerTestFixture, ReaderUnplugDoesNotReReadSurvivingCard)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A", "Reader B"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Both cards inserted
    mock->pushStatusChange({SCARD_S_SUCCESS,
                            {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16),
                             SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0 /* PnP */},
                            false});

    // PnP change (reader B unplugged) — triggers re-enumeration.
    // Also update reader list to only Reader A.
    MockPCSCScanProvider::StatusChangeAction readerChange;
    readerChange.returnValue = SCARD_S_SUCCESS;
    readerChange.eventStates = {0, 0, SCARD_STATE_CHANGED}; // PnP slot changed
    readerChange.newReaders = std::vector<std::string>{"Reader A"};
    mock->pushStatusChange(std::move(readerChange));

    // After re-enumeration with Reader A only: cancel
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // We should see: CardInserted(A), CardInserted(B) from initial detection.
    // After re-enumeration, Reader A's card should NOT be re-read (no duplicate CardInserted).
    ASSERT_EQ(result.events.size(), 2u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[0].readerName, "Reader A");
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].readerName, "Reader B");
}

// --- Test: Two readers, one unplugged, card removed from surviving reader detected ---
TEST_F(ScannerTestFixture, ReaderUnplugThenCardRemoveFromSurvivor)
{
    auto mock = std::make_unique<MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A", "Reader B"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Both cards inserted
    mock->pushStatusChange({SCARD_S_SUCCESS,
                            {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16),
                             SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0},
                            false});

    // Reader B unplugged — triggers re-enumeration
    MockPCSCScanProvider::StatusChangeAction readerChange;
    readerChange.returnValue = SCARD_S_SUCCESS;
    readerChange.eventStates = {0, 0, SCARD_STATE_CHANGED};
    readerChange.newReaders = std::vector<std::string>{"Reader A"};
    mock->pushStatusChange(std::move(readerChange));

    // After re-enumeration: card removed from Reader A (new event)
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED | (2 << 16), 0}, false});

    // Stop
    mock->pushStatusChange({SCARD_E_CANCELLED, {}, false});

    auto result = runScanner(std::move(mock));

    // CardInserted(A), CardInserted(B), then after re-enum: CardRemoved(A)
    ASSERT_EQ(result.events.size(), 3u);
    EXPECT_EQ(result.events[0].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[0].readerName, "Reader A");
    EXPECT_EQ(result.events[1].eventType, SmartCardEvent::CardInserted);
    EXPECT_EQ(result.events[1].readerName, "Reader B");
    EXPECT_EQ(result.events[2].eventType, SmartCardEvent::CardRemoved);
    EXPECT_EQ(result.events[2].readerName, "Reader A");
}
