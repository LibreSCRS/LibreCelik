// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDSCANNER_H
#define SMARTCARDSCANNER_H

#include <QObject>
#include "PCSC/pcsclite.h"

class SmartCardEvent;

class SmartCardScanner : public QObject
{
    Q_OBJECT
public:
    explicit SmartCardScanner(QObject *parent = nullptr);

public slots:
    void doWork();
    void requestStop();

signals:
    void smartCardReaderEnumerationChanged(QStringList scrNames);
    void smartCardEventOccured(SmartCardEvent sce);

private:
    SCARDCONTEXT hContext;
};

#endif // SMARTCARDSCANNER_H
