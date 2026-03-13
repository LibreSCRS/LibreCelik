// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTH_H
#define HEALTH_H

#include <QWidget>
#include <healthcard/healthtypes.h>
#include "document/document.h"

class HealthReader;
class QPushButton;
class TokenSection;

namespace Ui {
class Health;
}

class Health : public Document
{
    Q_OBJECT
public:
    explicit Health(std::string reader, QWidget* parent = nullptr);
    ~Health();

private slots:
    void healthDataReceived(const healthcard::HealthDocumentData& data);
    void openChangePinDlg();
    void printDocument();

private:
    Ui::Health* ui;

    healthcard::HealthDocumentData healthData;

    using HealthReaderUPtr = std::unique_ptr<HealthReader>;
    HealthReaderUPtr healthReader;

    TokenSection* tokenSection = nullptr;
    QPushButton* printButton = nullptr;
};

#endif // HEALTH_H
