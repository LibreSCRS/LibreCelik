// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "qsmartcardmonitor.h"

#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <string>
#include <vector>

QSmartCardMonitor::QSmartCardMonitor(LibreSCRS::SmartCard::MonitorService& monitor, QObject* parent)
    : QObject(parent), monitor(monitor)
{
    QPointer<QSmartCardMonitor> self = this;

    // Register the reader-list snapshot subscription FIRST so it becomes the
    // first subscriber and triggers poll-thread start. The bootstrap fire
    // path inside subscribeReaderList then runs on the poll thread (via the
    // internal readersCb → diffReadersAndDispatch → dispatchReaderListSnapshot
    // cycle) rather than as a synchronous call from the registering thread
    // against an uninitialised LM-internal reader-list state. Per-event subscription follows
    // so card events are serviced on the same poll thread that the reader-list
    // subscription started.
    readerListSubscriptionId = monitor.subscribeReaderList([self](const std::vector<std::string>& readers) {
        if (!self)
            return;
        QStringList qReaders;
        qReaders.reserve(static_cast<qsizetype>(readers.size()));
        for (const auto& r : readers)
            qReaders << QString::fromStdString(r);
        QMetaObject::invokeMethod(
            self,
            [self, qReaders]() {
                if (!self)
                    return;
                emit self->readerListChanged(qReaders);
            },
            Qt::QueuedConnection);
    });

    eventSubscriptionId = monitor.subscribe([self](const LibreSCRS::SmartCard::MonitorEvent& event) {
        if (!self)
            return;
        // Marshal onto the QObject's thread before emitting signals — the
        // public MonitorService invokes callbacks on its internal poll thread.
        QMetaObject::invokeMethod(
            self,
            [self, event]() {
                if (!self)
                    return;
                using Kind = LibreSCRS::SmartCard::MonitorEvent::Kind;
                switch (event.kind) {
                case Kind::ReaderAdded:
                case Kind::ReaderRemoved:
                    // Reader-list changes are surfaced via the snapshot
                    // subscription registered above; ignored here to avoid double-fire.
                    break;
                case Kind::CardInserted:
                case Kind::CardRemoved:
                    emit self->cardEvent(event);
                    break;
                case Kind::Error:
                    // PC/SC errors are rare; the next reader-list snapshot
                    // typically follows a recovery cycle. Log a breadcrumb
                    // rather than silently dropping so operators have
                    // something to correlate with if a reader misbehaves in
                    // the wild. Uncategorised qWarning — this source is
                    // shared between the main app and QSmartCardMonitorTests;
                    // the latter does not link the LC logging-category unit.
                    qWarning() << "QSmartCardMonitor: reader" << QString::fromStdString(event.readerName)
                               << "reported error:" << QString::fromStdString(event.diagnosticDetail.value_or(""));
                    break;
                }
            },
            Qt::QueuedConnection);
    });
}

QSmartCardMonitor::~QSmartCardMonitor()
{
    // Use FireAndForget (default) on both handles rather than Drain because:
    //   1. Both lambdas capture QPointer<this> (weak), so any in-flight
    //      callback that fires after destruction will detect the dangling
    //      pointer and early-out without dispatching.
    //   2. The marshalled body uses Qt::QueuedConnection, and Qt drops
    //      queued events whose receiver has been destroyed.
    // Together these eliminate the drain race window for both subscriptions.
    //
    // CAUTION: if a future refactor captures `this` raw or invokes the
    // callback synchronously, this safety vanishes and the destructor
    // MUST pass DrainPolicy::Drain on both unsubscribe calls.
    monitor.unsubscribeReaderList(readerListSubscriptionId);
    monitor.unsubscribe(eventSubscriptionId);
}
