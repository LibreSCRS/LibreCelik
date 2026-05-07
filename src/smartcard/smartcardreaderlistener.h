// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#pragma once

#include <LibreSCRS/SmartCard/MonitorService.h>

#include <QObject>
#include <QString>

#include <memory>

#include "qsmartcardmonitor.h"

/// Process-wide PC/SC monitor relay.
///
/// Singleton-like access goes through smartCardReaderListener() (a
/// Q_GLOBAL_STATIC handle), matching the LibreCelik singleton convention.
/// Q_GLOBAL_STATIC handles deterministic destruction at QApplication exit,
/// so no explicit shutdown() method is required (compare main()'s
/// aboutToQuit handler previously responsible for tearing down a Meyers
/// static).
class SmartCardReaderListener : public QObject
{
    Q_OBJECT
public:
    explicit SmartCardReaderListener(QObject* parent = nullptr);
    ~SmartCardReaderListener() override;

    SmartCardReaderListener(const SmartCardReaderListener&) = delete;
    SmartCardReaderListener& operator=(const SmartCardReaderListener&) = delete;

signals:
    void smartCardReaderEnumerationChanged(const QStringList& scrNames);
    void smartCardReaderEventOccured(LibreSCRS::SmartCard::MonitorEvent event);

private:
    std::unique_ptr<LibreSCRS::SmartCard::MonitorService> monitor;
    QSmartCardMonitor* qtMonitor = nullptr;
};

/// Returns the process-wide SmartCardReaderListener.
///
/// Backed by Q_GLOBAL_STATIC; first call constructs the listener,
/// QApplication shutdown destroys it deterministically before static
/// destructors run.
SmartCardReaderListener* smartCardReaderListener();
