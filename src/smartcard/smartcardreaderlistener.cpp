// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "smartcardreaderlistener.h"
#include "qsmartcardmonitor.h"
#include "utils/libreceliklog.h"

SmartCardReaderListener& SmartCardReaderListener::instance()
{
    static SmartCardReaderListener scrl;
    return scrl;
}

SmartCardReaderListener::SmartCardReaderListener(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<smartcard::MonitorEvent>("smartcard::MonitorEvent");

    monitor = std::make_unique<smartcard::Monitor>();
    qtMonitor = new QSmartCardMonitor(*monitor, this);

    connect(qtMonitor, &QSmartCardMonitor::readerListChanged, this, [this](const QStringList& readers) {
        qCDebug(librecSCRSCard) << "SmartCardListener (main thread) got readers:";
        for (const auto& r : readers)
            qCInfo(librecSCRSCard) << "    " << r;
        emit smartCardReaderEnumerationChanged(readers);
    });

    connect(qtMonitor, &QSmartCardMonitor::cardEvent, this, [this](const smartcard::MonitorEvent& event) {
        qCDebug(librecSCRSCard) << "SmartCardListener received event from the reader:"
                                << QString::fromStdString(event.readerName) << "Type:"
                                << (event.type == smartcard::MonitorEvent::Type::CardInserted ? "CardInserted"
                                                                                              : "CardRemoved");
        emit smartCardReaderEventOccured(event);
    });
}

void SmartCardReaderListener::shutdown()
{
    // Explicitly destroy monitor before static destruction order issues.
    // QSmartCardMonitor unsubscribes in its destructor (child QObject, destroyed first),
    // then Monitor joins the thread.
    delete qtMonitor;
    qtMonitor = nullptr;
    monitor.reset();
}

SmartCardReaderListener::~SmartCardReaderListener() = default;
