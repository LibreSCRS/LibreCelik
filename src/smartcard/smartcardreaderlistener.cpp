// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "smartcardreaderlistener.h"
#include "qsmartcardmonitor.h"
#include "utils/libreceliklog.h"

#include <QGlobalStatic>

namespace {
Q_GLOBAL_STATIC(SmartCardReaderListener, gSmartCardReaderListener)
} // namespace

SmartCardReaderListener* smartCardReaderListener()
{
    return gSmartCardReaderListener;
}

SmartCardReaderListener::SmartCardReaderListener(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<LibreSCRS::SmartCard::MonitorEvent>("LibreSCRS::SmartCard::MonitorEvent");

    monitor = std::make_unique<LibreSCRS::SmartCard::MonitorService>();
    qtMonitor = new QSmartCardMonitor(*monitor, this);

    connect(qtMonitor, &QSmartCardMonitor::readerListChanged, this, [this](const QStringList& readers) {
        qCDebug(lcSmartCard) << "SmartCardListener (main thread) got readers:";
        for (const auto& r : readers)
            qCInfo(lcSmartCard) << "    " << r;
        emit smartCardReaderEnumerationChanged(readers);
    });

    connect(qtMonitor, &QSmartCardMonitor::cardEvent, this, [this](const LibreSCRS::SmartCard::MonitorEvent& event) {
        using Kind = LibreSCRS::SmartCard::MonitorEvent::Kind;
        qCDebug(lcSmartCard) << "SmartCardListener received event from the reader:"
                             << QString::fromStdString(event.readerName)
                             << "Kind:" << (event.kind == Kind::CardInserted ? "CardInserted" : "CardRemoved");
        emit smartCardReaderEventOccured(event);
    });
}

SmartCardReaderListener::~SmartCardReaderListener()
{
    // QSmartCardMonitor (child QObject) is destroyed first, unsubscribing the
    // PC/SC observer. MonitorService joins its worker thread on reset(). Q_GLOBAL_STATIC
    // ensures we run before QApplication-managed static-destructor noise, so this
    // ordering is deterministic.
    delete qtMonitor;
    qtMonitor = nullptr;
    monitor.reset();
}
