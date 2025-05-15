// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDREADERLISTENER_H
#define SMARTCARDREADERLISTENER_H

#include <QString>
#include <QObject>
#include "smartcardevent.h"

class QThread;
class SmartCardScanner;

class SmartCardReaderListener : public QObject
{
    Q_OBJECT
public:
    static SmartCardReaderListener& instance();

signals:
    void smartCardReaderEnumerationChanged(QStringList scrNames);
    void smartCardReaderEventOccured(SmartCardEvent sce);

private:
    SmartCardReaderListener(QObject *parent = nullptr);
    ~SmartCardReaderListener();
    SmartCardReaderListener( const SmartCardReaderListener& ) = delete;
    SmartCardReaderListener& operator=( const SmartCardReaderListener& ) = delete;

private:
    QThread* scannerThread;
    SmartCardScanner* scScanner;

private slots:
    // Slots for signals from scanner thread
    void onSmartCardReaderEnumerationChanged(const QStringList& scrNames);
    void onSmartCardEventOccured(const SmartCardEvent& sce);
};

#endif // SMARTCARDREADERLISTENER_H
