// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdauthdlg.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

EMRTDAuthDlg::EMRTDAuthDlg(bool paceSupported, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-emrtd-auth-dlg-title"));
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);

    auto* hintLabel = new QLabel(qtTrId("lc-emrtd-insert-mrz-hint"), this);
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    // Radio buttons for auth mode
    auto* radioLayout = new QHBoxLayout();
    canRadio = new QRadioButton(qtTrId("lc-emrtd-auth-mode-can"), this);
    mrzRadio = new QRadioButton(qtTrId("lc-emrtd-auth-mode-mrz"), this);
    radioLayout->addWidget(canRadio);
    radioLayout->addWidget(mrzRadio);
    layout->addLayout(radioLayout);

    // Stacked widget for CAN / MRZ input pages
    inputStack = new QStackedWidget(this);

    // Page 0: CAN input
    auto* canPage = new QWidget();
    auto* canLayout = new QVBoxLayout(canPage);
    auto* canLabel = new QLabel(qtTrId("lc-emrtd-can-label"), canPage);
    canEdit = new QLineEdit(canPage);
    canEdit->setMaxLength(6);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    canEdit->setPlaceholderText("000000");
    canLayout->addWidget(canLabel);
    canLayout->addWidget(canEdit);
    canLayout->addStretch();
    inputStack->addWidget(canPage);

    // Page 1: MRZ input
    auto* mrzPage = new QWidget();
    auto* mrzLayout = new QVBoxLayout(mrzPage);

    auto* docNumberLabel = new QLabel(qtTrId("lc-emrtd-mrz-doc-number"), mrzPage);
    docNumberEdit = new QLineEdit(mrzPage);
    docNumberEdit->setMaxLength(9);
    mrzLayout->addWidget(docNumberLabel);
    mrzLayout->addWidget(docNumberEdit);

    auto* dobLabel = new QLabel(qtTrId("lc-emrtd-mrz-dob"), mrzPage);
    dobEdit = new QLineEdit(mrzPage);
    dobEdit->setMaxLength(6);
    dobEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    dobEdit->setPlaceholderText("YYMMDD");
    mrzLayout->addWidget(dobLabel);
    mrzLayout->addWidget(dobEdit);

    auto* expiryLabel = new QLabel(qtTrId("lc-emrtd-mrz-expiry"), mrzPage);
    expiryEdit = new QLineEdit(mrzPage);
    expiryEdit->setMaxLength(6);
    expiryEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    expiryEdit->setPlaceholderText("YYMMDD");
    mrzLayout->addWidget(expiryLabel);
    mrzLayout->addWidget(expiryEdit);

    mrzLayout->addStretch();
    inputStack->addWidget(mrzPage);

    layout->addWidget(inputStack);

    // Status label
    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);
    layout->addWidget(statusLabel);

    // Buttons
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    authButton = buttonBox->addButton(qtTrId("lc-emrtd-authenticate"), QDialogButtonBox::AcceptRole);
    authButton->setEnabled(false);
    layout->addWidget(buttonBox);

    // Connections
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(authButton, &QPushButton::clicked, this, &EMRTDAuthDlg::onAuthenticateClicked);
    connect(canRadio, &QRadioButton::toggled, this, &EMRTDAuthDlg::onAuthModeChanged);
    connect(mrzRadio, &QRadioButton::toggled, this, &EMRTDAuthDlg::onAuthModeChanged);

    connect(canEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);
    connect(docNumberEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);
    connect(dobEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);
    connect(expiryEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);

    // Default to CAN (printed on card, user-friendly)
    canRadio->setChecked(true);
}

void EMRTDAuthDlg::onAuthenticateClicked()
{
    authButton->setEnabled(false);
    statusLabel->setStyleSheet("color: #E6873C;");
    statusLabel->setText(qtTrId("lc-emrtd-authenticating"));
    statusLabel->setVisible(true);

    QMap<QString, QString> credentials;
    if (canRadio->isChecked()) {
        credentials["can"] = canEdit->text();
    } else {
        credentials["mrz_doc_number"] = docNumberEdit->text();
        credentials["mrz_dob"] = dobEdit->text();
        credentials["mrz_expiry"] = expiryEdit->text();
    }

    emit credentialsEntered(credentials);
}

void EMRTDAuthDlg::onAuthSuccess()
{
    accept();
}

void EMRTDAuthDlg::onAuthFailed(const QString& errorMessage)
{
    statusLabel->setStyleSheet("color: red;");
    statusLabel->setText(errorMessage);
    statusLabel->setVisible(true);
    authButton->setEnabled(true);
}

void EMRTDAuthDlg::onAuthModeChanged()
{
    if (canRadio->isChecked()) {
        inputStack->setCurrentIndex(0);
    } else {
        inputStack->setCurrentIndex(1);
    }
    validateForm();
}

void EMRTDAuthDlg::validateForm()
{
    bool valid = false;
    if (canRadio->isChecked()) {
        valid = canEdit->text().length() == 6;
    } else {
        valid = !docNumberEdit->text().isEmpty() && dobEdit->text().length() == 6 && expiryEdit->text().length() == 6;
    }
    authButton->setEnabled(valid);
}
