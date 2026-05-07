// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "qsmartcardmonitor.h"

#include <QDebug>
#include <QMetaObject>
#include <QPointer>

QSmartCardMonitor::QSmartCardMonitor(LibreSCRS::SmartCard::MonitorService& monitor, QObject* parent)
    : QObject(parent), monitor(monitor)
{
    QPointer<QSmartCardMonitor> self = this;
    subscriptionId = monitor.subscribe([self](const LibreSCRS::SmartCard::MonitorEvent& event) {
        if (!self)
            return;
        // Marshal onto the QObject's thread before mutating knownReaders or
        // emitting signals — the public MonitorService invokes callbacks on its
        // internal poll thread.
        QMetaObject::invokeMethod(
            self,
            [self, event]() {
                if (!self)
                    return;
                using Kind = LibreSCRS::SmartCard::MonitorEvent::Kind;
                switch (event.kind) {
                case Kind::ReaderAdded:
                    self->knownReaders.insert(event.readerName);
                    {
                        QStringList qReaders;
                        qReaders.reserve(static_cast<qsizetype>(self->knownReaders.size()));
                        for (const auto& r : self->knownReaders)
                            qReaders << QString::fromStdString(r);
                        emit self->readerListChanged(qReaders);
                    }
                    break;
                case Kind::ReaderRemoved:
                    self->knownReaders.erase(event.readerName);
                    {
                        QStringList qReaders;
                        qReaders.reserve(static_cast<qsizetype>(self->knownReaders.size()));
                        for (const auto& r : self->knownReaders)
                            qReaders << QString::fromStdString(r);
                        emit self->readerListChanged(qReaders);
                    }
                    break;
                case Kind::CardInserted:
                case Kind::CardRemoved:
                    emit self->cardEvent(event);
                    break;
                case Kind::Error:
                    // PC/SC errors are rare; the next ReaderAdded/Removed
                    // cycle typically recovers. Log a breadcrumb rather than
                    // silently dropping so operators have something to
                    // correlate with if a reader misbehaves in the wild.
                    // Uncategorised qWarning — this source is shared between
                    // the main app and QSmartCardMonitorTests; the latter
                    // does not link the LC logging-category unit.
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
    // Use unsubscribe() rather than unsubscribeAndDrain() because:
    //   1. The lambda registered in the ctor captures QPointer<this> (weak),
    //      so any in-flight callback that fires after destruction will
    //      detect the dangling pointer and early-out without dispatching.
    //   2. The lambda uses Qt::QueuedConnection to marshal events back to
    //      the Qt main thread, and Qt drops queued events whose receiver
    //      has been destroyed.
    // Together these eliminate the unsubscribeAndDrain race window.
    //
    // CAUTION: if a future refactor captures `this` raw or invokes the
    // callback synchronously, this safety vanishes and the destructor
    // MUST switch to unsubscribeAndDrain.
    monitor.unsubscribe(subscriptionId);
}
