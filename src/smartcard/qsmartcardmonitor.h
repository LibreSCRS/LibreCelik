// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#ifndef QSMARTCARDMONITOR_H
#define QSMARTCARDMONITOR_H

#include <QMetaType>
#include <QObject>
#include <QStringList>
#include <smartcard/monitor.h>

Q_DECLARE_METATYPE(smartcard::MonitorEvent)

class QSmartCardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit QSmartCardMonitor(smartcard::Monitor& monitor, QObject* parent = nullptr);
    ~QSmartCardMonitor() override;

signals:
    void cardEvent(const smartcard::MonitorEvent& event);
    void readerListChanged(const QStringList& readers);

private:
    smartcard::Monitor& monitor;
    smartcard::Monitor::SubscriptionId subscriptionId;
};

#endif // QSMARTCARDMONITOR_H
