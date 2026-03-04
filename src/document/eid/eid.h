// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EID_H
#define EID_H

#include <QWidget>
#include <eidcard/eidtypes.h>
#include "document/document.h"
#include <string>

class EIdReader;
class QLabel;
class QPushButton;
class TokenSection;

namespace Ui {
class EId;
}

class EId : public Document
{
    Q_OBJECT
public:
    explicit EId(std::string reader, QWidget *parent = nullptr);
    ~EId();

private slots:
    void cardTypeReceived(const eidcard::CardType& data);
    void fixedPersonalDataReceived(const eidcard::FixedPersonalData& data);
    void variablePersonalDataReceived(const eidcard::VariablePersonalData& data);
    void documentDataReceived(const eidcard::DocumentData& data);
    void photoDataReceived(const eidcard::PhotoData& data);
    void cardVerificationResultReceived(const eidcard::VerificationResult& data);
    void fixedVerificationResultReceived(const eidcard::VerificationResult& data);
    void variableVerificationResultReceived(const eidcard::VerificationResult& data);

    void openChangePinDlg();
    void printDocument();

private:
    void updateVerificationIcons(const eidcard::VerificationResult& data, QLabel* iconLabel);
    void applyCardTypeVisibility();
    QString getBase64Photo();
    QString assembleAddress(const eidcard::VariablePersonalData& vpd) const;
    QString assemblePlaceOfBirth(const eidcard::FixedPersonalData& fpd) const;

private:
    Ui::EId *ui;

    eidcard::CardType cardType = eidcard::CardType::Unknown;
    eidcard::FixedPersonalData fixedPersonalData;
    eidcard::VariablePersonalData variablePersonalData;
    eidcard::DocumentData documentData;

    using EIdReaderUPtr = std::unique_ptr<EIdReader>;
    EIdReaderUPtr eidReader;

    TokenSection* tokenSection = nullptr;
    QPushButton* printButton = nullptr;
};

#endif // EID_H
