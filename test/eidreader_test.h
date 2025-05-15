// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDREADER_TEST_H
#define EIDREADER_TEST_H

#include "document/eid/eidreader.h"

class EIdReaderListener : public QObject
{
    Q_OBJECT
public:
    EIdReaderListener(QObject* parent = nullptr);
    ~EIdReaderListener();

protected:
    std::unique_ptr<EIdReader> eIDReader;
};

#endif // EIDREADER_TEST_H
