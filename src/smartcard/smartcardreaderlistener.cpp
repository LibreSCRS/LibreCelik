// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "smartcardreaderlistener.h"
#include "smartcardscanner.h"
#include "utils/libreceliklog.h"
#include <QThread>

SmartCardReaderListener& SmartCardReaderListener::instance()
{
    static SmartCardReaderListener scrl;
    return scrl;
}

SmartCardReaderListener::SmartCardReaderListener(QObject *parent) : QObject(parent)
{
    scannerThread = new QThread(this);
    scScanner = new SmartCardScanner;
    scScanner->moveToThread(scannerThread);

    connect(scannerThread, &QThread::started, scScanner, &SmartCardScanner::doWork);
    connect(scannerThread, &QThread::finished, scScanner, &QObject::deleteLater);
    connect(scannerThread, &QThread::finished, scannerThread, &QObject::deleteLater);
    connect(scScanner, &SmartCardScanner::smartCardReaderEnumerationChanged, this, &SmartCardReaderListener::onSmartCardReaderEnumerationChanged);
    connect(scScanner, &SmartCardScanner::smartCardEventOccured, this, &SmartCardReaderListener::onSmartCardEventOccured);

    if(!scannerThread->isRunning())
    {
        scannerThread->start();
    }
}

SmartCardReaderListener::~SmartCardReaderListener()
{
    if(scannerThread->isRunning())
    {
        scScanner->requestStop();
        scannerThread->requestInterruption();
        scannerThread->quit();
        scannerThread->wait();
    }
}

// Received from sc scanner
void SmartCardReaderListener::onSmartCardReaderEnumerationChanged(const QStringList& scrNames)
{
    qCDebug(librecSCRSCard) << "SmartCardListener (main thread) got readers: ";
    for (auto& scrName : scrNames)
        qCInfo(librecSCRSCard) << "    " << scrName;

    emit smartCardReaderEnumerationChanged(scrNames);
}

// Received from sc scanner
void SmartCardReaderListener::onSmartCardEventOccured(const SmartCardEvent &sce)
{
    qCDebug(librecSCRSCard) << "SmartCardListener received event from the reader: "
                          << sce.readerName.c_str()
                          << " Event: "
                          << sce.eventType;

    emit smartCardReaderEventOccured(sce);
}
