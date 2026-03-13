// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDSCANNER_H
#define SMARTCARDSCANNER_H

#include <QObject>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif
#include "smartcardevent.h"

class IPCSCScanProvider;

class SmartCardScanner : public QObject
{
    Q_OBJECT
public:
    explicit SmartCardScanner(QObject* parent = nullptr);
    explicit SmartCardScanner(std::unique_ptr<IPCSCScanProvider> provider, QObject* parent = nullptr);
    ~SmartCardScanner() override;

public slots:
    void doWork();
    void requestStop();

signals:
    void smartCardReaderEnumerationChanged(QStringList scrNames);
    void smartCardEventOccured(SmartCardEvent sce);

private:
    void establishContext();
    bool checkPnPSupport();
    std::vector<std::string> enumerateReaders();
    void waitForFirstReader(bool pnp);
    bool processEvents(std::vector<SCARD_READERSTATE>& states, int readerCount, bool pnp);

    std::unique_ptr<IPCSCScanProvider> pcsc;
    SCARDCONTEXT hContext = 0;
    std::map<std::string, DWORD> previousReaderStates;
};

#endif // SMARTCARDSCANNER_H
