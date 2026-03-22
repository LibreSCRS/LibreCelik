// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "qsmartcardmonitor.h"
#include <QMetaObject>
#include <QPointer>

QSmartCardMonitor::QSmartCardMonitor(smartcard::Monitor& monitor, QObject* parent) : QObject(parent), monitor(monitor)
{
    QPointer<QSmartCardMonitor> self = this;
    subscriptionId = monitor.subscribe(
        [self](const smartcard::MonitorEvent& event) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, event]() {
                    if (!self)
                        return;
                    emit self->cardEvent(event);
                },
                Qt::QueuedConnection);
        },
        [self](const std::vector<std::string>& readers) {
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
}

QSmartCardMonitor::~QSmartCardMonitor()
{
    monitor.unsubscribe(subscriptionId);
}
