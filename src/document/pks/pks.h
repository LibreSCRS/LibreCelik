// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PKS_H
#define PKS_H

#include <memory>
#include <string>
#include <QWidget>
#include "document/document.h"

class PKSReader;
class TokenSection;

class Pks : public Document
{
    Q_OBJECT
public:
    explicit Pks(std::string reader, QWidget* parent = nullptr);
    ~Pks();

private slots:
    void openChangePinDlg();

private:
    using PKSReaderUPtr = std::unique_ptr<PKSReader>;
    PKSReaderUPtr pksReader;

    TokenSection* tokenSection = nullptr;
};

#endif // PKS_H
