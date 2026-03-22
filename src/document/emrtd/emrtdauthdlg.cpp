// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdauthdlg.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
QString padRight(const QString& s, int len, QChar fill = '<')
{
    if (s.length() >= len)
        return s.left(len);
    return s + QString(len - s.length(), fill);
}
} // namespace

EMRTDAuthDlg::EMRTDAuthDlg(bool paceSupported, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-emrtd-auth-dlg-title"));
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Radio buttons
    auto* radioLayout = new QHBoxLayout();
    radioLayout->setSpacing(16);
    canRadio = new QRadioButton(qtTrId("lc-emrtd-auth-can-title") + " — " + qtTrId("lc-emrtd-auth-can-desc"), this);
    mrzRadio = new QRadioButton(qtTrId("lc-emrtd-auth-mrz-title") + " — " + qtTrId("lc-emrtd-auth-mrz-desc"), this);
    radioLayout->addWidget(canRadio);
    radioLayout->addWidget(mrzRadio);
    layout->addLayout(radioLayout);

    // Stacked widget
    inputStack = new QStackedWidget(this);

    // --- Page 0: CAN ---
    auto* canPage = new QWidget();
    auto* canLayout = new QVBoxLayout(canPage);
    canLayout->setContentsMargins(0, 8, 0, 0);

    auto* canHintLabel = new QLabel(qtTrId("lc-emrtd-auth-can-desc"), canPage);
    canHintLabel->setStyleSheet("color: #888; font-size: 10px;");
    canLayout->addWidget(canHintLabel);

    canEdit = new QLineEdit(canPage);
    canEdit->setMaxLength(6);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    canEdit->setPlaceholderText("000000");
    canEdit->setStyleSheet("font-family: monospace; font-size: 20px; letter-spacing: 10px;"
                           "padding: 8px; text-align: center;");
    canEdit->setAlignment(Qt::AlignCenter);
    canLayout->addWidget(canEdit);
    canLayout->addStretch();
    inputStack->addWidget(canPage);

    // --- Page 1: MRZ ---
    auto* mrzPage = new QWidget();
    auto* mrzLayout = new QVBoxLayout(mrzPage);
    mrzLayout->setContentsMargins(0, 8, 0, 0);
    mrzLayout->setSpacing(8);

    auto* docNumLabel = new QLabel(qtTrId("lc-emrtd-auth-mrz-docnum"), mrzPage);
    docNumLabel->setStyleSheet("color: #888; font-size: 10px;");
    docNumberEdit = new QLineEdit(mrzPage);
    docNumberEdit->setMaxLength(9);
    mrzLayout->addWidget(docNumLabel);
    mrzLayout->addWidget(docNumberEdit);

    auto* dateRow = new QHBoxLayout();
    dateRow->setSpacing(8);

    auto* dobBox = new QVBoxLayout();
    auto* dobLabel = new QLabel(qtTrId("lc-emrtd-auth-mrz-dob"), mrzPage);
    dobLabel->setStyleSheet("color: #888; font-size: 10px;");
    dobEdit = new QLineEdit(mrzPage);
    dobEdit->setInputMask("99.99.9999");
    dobEdit->setPlaceholderText("DD.MM.YYYY");
    dobBox->addWidget(dobLabel);
    dobBox->addWidget(dobEdit);
    dateRow->addLayout(dobBox);

    auto* expiryBox = new QVBoxLayout();
    auto* expiryLabel = new QLabel(qtTrId("lc-emrtd-auth-mrz-expiry"), mrzPage);
    expiryLabel->setStyleSheet("color: #888; font-size: 10px;");
    expiryEdit = new QLineEdit(mrzPage);
    expiryEdit->setInputMask("99.99.9999");
    expiryEdit->setPlaceholderText("DD.MM.YYYY");
    expiryBox->addWidget(expiryLabel);
    expiryBox->addWidget(expiryEdit);
    dateRow->addLayout(expiryBox);

    mrzLayout->addLayout(dateRow);

    // MRZ preview
    mrzPreview = new QLabel(mrzPage);
    mrzPreview->setStyleSheet("font-family: monospace; font-size: 10px; color: #7CB;"
                              "background: rgba(0,0,0,0.3); border-radius: 4px; padding: 8px;");
    mrzPreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mrzLayout->addWidget(mrzPreview);

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
    connect(docNumberEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::updateMrzPreview);
    connect(dobEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);
    connect(dobEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::updateMrzPreview);
    connect(expiryEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::validateForm);
    connect(expiryEdit, &QLineEdit::textChanged, this, &EMRTDAuthDlg::updateMrzPreview);

    // Default: always CAN (user-friendly, printed on card)
    canRadio->setChecked(true);

    updateMrzPreview();
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
        credentials["mrz_dob"] = dateToYYMMDD(dobEdit->text());
        credentials["mrz_expiry"] = dateToYYMMDD(expiryEdit->text());
    }

    emit credentialsEntered(credentials);
}

void EMRTDAuthDlg::onAuthSuccess()
{
    accept();
}

void EMRTDAuthDlg::onAuthFailed(const QString& errorMessage)
{
    statusLabel->setStyleSheet("color: #CC3333;");
    statusLabel->setText(errorMessage);
    statusLabel->setVisible(true);
    authButton->setEnabled(true);
}

void EMRTDAuthDlg::onAuthModeChanged()
{
    inputStack->setCurrentIndex(canRadio->isChecked() ? 0 : 1);
    validateForm();
}

void EMRTDAuthDlg::validateForm()
{
    bool valid = false;
    if (canRadio->isChecked()) {
        valid = canEdit->text().length() == 6;
    } else {
        valid = !docNumberEdit->text().isEmpty() && QDate::fromString(dobEdit->text(), "dd.MM.yyyy").isValid()
                && QDate::fromString(expiryEdit->text(), "dd.MM.yyyy").isValid();
    }
    authButton->setEnabled(valid);
}

void EMRTDAuthDlg::updateMrzPreview()
{
    QString docNum = padRight(docNumberEdit->text().toUpper(), 9);
    QString dob = dateToYYMMDD(dobEdit->text());
    if (dob.isEmpty())
        dob = "<<<<<<";
    QString expiry = dateToYYMMDD(expiryEdit->text());
    if (expiry.isEmpty())
        expiry = "<<<<<<";

    QString line1 = padRight("I<SRV" + docNum, 30);
    QString line2 = padRight(dob + "<" + expiry + "SRV", 30);
    mrzPreview->setText(line1 + "\n" + line2);
}

QString EMRTDAuthDlg::dateToYYMMDD(const QString& ddmmyyyy) const
{
    QDate d = QDate::fromString(ddmmyyyy, "dd.MM.yyyy");
    if (!d.isValid())
        return {};
    return d.toString("yyMMdd");
}
