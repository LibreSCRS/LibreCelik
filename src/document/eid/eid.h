// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EID_H
#define EID_H

#include <QWidget>
#include "celikapi/celikapiplus.h"
#include "document/document.h"

class EIdReader;
class QLabel;
class QLineEdit;

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
    void cardVersionReceived(const CelikAPI::CardVersion& data);
    void fixedPersonalDataReceived(const CelikAPI::FixedPersonalData& data);
    void varibalePersonalDataReceived(const CelikAPI::VariablePersonalData& data);
    void documentDataReceived(const CelikAPI::DocumentData& data);
    void photoDataReceived(const CelikAPI::PhotoData& data);
    void cardSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data);
    void fixedSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data);
    void variableSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data);

    void on_toolButton_clicked();

private:
    void updateVerificationIcons(const CelikAPI::VerificationResult& data, QLabel* iconLabel);
    void showLabelAndLineEdit(QLabel* label, QLineEdit* lineEdit, bool show);
    QString getBase64Photo();

private:
    Ui::EId *ui;

    CelikAPI::CardVersion cardVersion;
    CelikAPI::FixedPersonalData fixedPersonalData;
    CelikAPI::VariablePersonalData variablePersonalData;
    CelikAPI::DocumentData documentData;

    using EIdReaderUPtr = std::unique_ptr<EIdReader>;
    EIdReaderUPtr eidReader;
};

#endif // EID_H
