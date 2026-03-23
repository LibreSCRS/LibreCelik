// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#pragma once

#include <QObject>
#include <QString>
#include <smartcard/monitor.h>
#include <smartcard/monitor_event.h>

#include <memory>

#include "qsmartcardmonitor.h"

class SmartCardReaderListener : public QObject
{
    Q_OBJECT
public:
    static SmartCardReaderListener& instance();
    void shutdown(); // Stop monitor before app exit

signals:
    void smartCardReaderEnumerationChanged(const QStringList& scrNames);
    void smartCardReaderEventOccured(smartcard::MonitorEvent event);

private:
    SmartCardReaderListener(QObject* parent = nullptr);
    ~SmartCardReaderListener() override;
    SmartCardReaderListener(const SmartCardReaderListener&) = delete;
    SmartCardReaderListener& operator=(const SmartCardReaderListener&) = delete;

private:
    std::unique_ptr<smartcard::Monitor> monitor;
    QSmartCardMonitor* qtMonitor = nullptr;
};
