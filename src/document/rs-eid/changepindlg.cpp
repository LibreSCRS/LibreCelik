// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "changepindlg.h"

#include "utils/iconutils.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QRegularExpressionValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ChangePinDlg::ChangePinDlg(const QString& pinLabel, bool isTransport, int minLen, int maxLen, QWidget* parent)
    : QDialog(parent), pinMinLength(minLen > 0 ? minLen : 4), pinMaxLength(maxLen > 0 ? maxLen : 8)
{
    setWindowTitle(isTransport ? qtTrId("lc-changepin-initialize-title").arg(pinLabel)
                               : qtTrId("lc-changepin-change-title").arg(pinLabel));
    setMinimumWidth(350);

    auto* layout = new QVBoxLayout(this);

    retriesLabel = new QLabel(qtTrId("lc-changepin-retries-remaining").arg("..."), this);
    layout->addWidget(retriesLabel);

    currentPinEdit = new QLineEdit(this);
    currentPinEdit->setEchoMode(QLineEdit::Password);
    currentPinEdit->setMaxLength(pinMaxLength);
    currentPinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), this));
    currentPinEdit->setPlaceholderText(isTransport ? qtTrId("lc-changepin-transport-placeholder")
                                                   : qtTrId("lc-changepin-current"));
    iconutils::addToggleVisibilityAction(this, currentPinEdit);
    layout->addWidget(currentPinEdit);

    newPinEdit = new QLineEdit(this);
    newPinEdit->setEchoMode(QLineEdit::Password);
    newPinEdit->setMaxLength(pinMaxLength);
    newPinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), this));
    newPinEdit->setPlaceholderText(qtTrId("lc-changepin-new"));
    iconutils::addToggleVisibilityAction(this, newPinEdit);
    layout->addWidget(newPinEdit);

    confirmPinEdit = new QLineEdit(this);
    confirmPinEdit->setEchoMode(QLineEdit::Password);
    confirmPinEdit->setMaxLength(pinMaxLength);
    confirmPinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), this));
    confirmPinEdit->setPlaceholderText(qtTrId("lc-changepin-confirm"));
    iconutils::addToggleVisibilityAction(this, confirmPinEdit);
    layout->addWidget(confirmPinEdit);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setEnabled(false);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(okButton, &QPushButton::clicked, this, &ChangePinDlg::onOkClicked);

    connect(currentPinEdit, &QLineEdit::textChanged, this, &ChangePinDlg::validateForm);
    connect(newPinEdit, &QLineEdit::textChanged, this, &ChangePinDlg::validateForm);
    connect(confirmPinEdit, &QLineEdit::textChanged, this, &ChangePinDlg::validateForm);
}

ChangePinDlg::~ChangePinDlg() = default;

void ChangePinDlg::onOkClicked()
{
    okButton->setEnabled(false);
    statusLabel->setStyleSheet("");
    statusLabel->setText(qtTrId("lc-changepin-changing"));

    emit pinChangeRequested(currentPinEdit->text(), newPinEdit->text());
}

void ChangePinDlg::onPinTriesLeftRead(int triesLeft, bool blocked)
{
    if (blocked) {
        retriesLabel->setText(qtTrId("lc-changepin-blocked"));
        retriesLabel->setStyleSheet("color: red; font-weight: bold;");
        okButton->setEnabled(false);
        currentPinEdit->setEnabled(false);
        newPinEdit->setEnabled(false);
        confirmPinEdit->setEnabled(false);
    } else if (triesLeft >= 0) {
        retriesLabel->setText(qtTrId("lc-changepin-retries-remaining").arg(triesLeft));
    } else {
        retriesLabel->setText(qtTrId("lc-changepin-retries-remaining").arg("?"));
    }
}

void ChangePinDlg::onPinChangeSuccess()
{
    statusLabel->setStyleSheet("color: green;");
    statusLabel->setText(qtTrId("lc-changepin-success"));
    okButton->setEnabled(false);
    // Change Cancel to Close
    buttonBox->button(QDialogButtonBox::Cancel)->setText(qtTrId("lc-changepin-close"));
}

void ChangePinDlg::onPinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage)
{
    statusLabel->setStyleSheet("color: red;");
    statusLabel->setText(errorMessage);

    // Refresh retries display
    onPinTriesLeftRead(retriesLeft, blocked);

    if (!blocked)
        validateForm();
}

void ChangePinDlg::validateForm()
{
    statusLabel->clear();
    statusLabel->setStyleSheet("");

    bool valid = isValidPinLength(currentPinEdit->text()) && isValidPinLength(newPinEdit->text()) &&
                 isValidPinLength(confirmPinEdit->text()) && newPinEdit->text() == confirmPinEdit->text();

    // Show hint when confirm doesn't match and both are filled
    if (isValidPinLength(newPinEdit->text()) && confirmPinEdit->text().length() >= pinMinLength &&
        newPinEdit->text() != confirmPinEdit->text()) {
        statusLabel->setStyleSheet("color: red;");
        statusLabel->setText(qtTrId("lc-changepin-mismatch"));
    }

    okButton->setEnabled(valid);
}

bool ChangePinDlg::isValidPinLength(const QString& pin) const
{
    return pin.length() >= pinMinLength && pin.length() <= pinMaxLength;
}
