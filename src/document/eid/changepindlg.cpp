// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "changepindlg.h"
#include "eidreader.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QIcon>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ChangePinDlg::ChangePinDlg(const std::string& readerName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-changepin-title"));
    setMinimumWidth(350);

    auto *layout = new QVBoxLayout(this);

    retriesLabel = new QLabel(qtTrId("lc-changepin-retries-remaining").arg("..."), this);
    layout->addWidget(retriesLabel);

    currentPinEdit = new QLineEdit(this);
    currentPinEdit->setEchoMode(QLineEdit::Password);
    currentPinEdit->setMaxLength(8);
    currentPinEdit->setValidator(new QIntValidator(0, 99999999, this));
    currentPinEdit->setPlaceholderText(qtTrId("lc-changepin-current"));
    addToggleVisibilityAction(currentPinEdit);
    layout->addWidget(currentPinEdit);

    newPinEdit = new QLineEdit(this);
    newPinEdit->setEchoMode(QLineEdit::Password);
    newPinEdit->setMaxLength(8);
    newPinEdit->setValidator(new QIntValidator(0, 99999999, this));
    newPinEdit->setPlaceholderText(qtTrId("lc-changepin-new"));
    addToggleVisibilityAction(newPinEdit);
    layout->addWidget(newPinEdit);

    confirmPinEdit = new QLineEdit(this);
    confirmPinEdit->setEchoMode(QLineEdit::Password);
    confirmPinEdit->setMaxLength(8);
    confirmPinEdit->setValidator(new QIntValidator(0, 99999999, this));
    confirmPinEdit->setPlaceholderText(qtTrId("lc-changepin-confirm"));
    addToggleVisibilityAction(confirmPinEdit);
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

    eidReader = std::make_unique<EIdReader>(readerName);

    connect(eidReader.get(), &EIdReader::pinTriesLeftRead, this, &ChangePinDlg::onPinTriesLeftRead);
    connect(eidReader.get(), &EIdReader::pinChangeSuccess, this, &ChangePinDlg::onPinChangeSuccess);
    connect(eidReader.get(), &EIdReader::pinChangeFailed, this, &ChangePinDlg::onPinChangeFailed);

    eidReader->requestPINTriesLeft();
}

ChangePinDlg::~ChangePinDlg() = default;

void ChangePinDlg::onOkClicked()
{
    okButton->setEnabled(false);
    statusLabel->setStyleSheet("");
    statusLabel->setText(qtTrId("lc-changepin-changing"));

    eidReader->requestChangePIN(currentPinEdit->text(), newPinEdit->text());
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

    bool valid = isValidPinLength(currentPinEdit->text())
              && isValidPinLength(newPinEdit->text())
              && isValidPinLength(confirmPinEdit->text())
              && newPinEdit->text() == confirmPinEdit->text();

    // Show hint when confirm doesn't match and both are filled
    if (isValidPinLength(newPinEdit->text())
        && confirmPinEdit->text().length() >= 4
        && newPinEdit->text() != confirmPinEdit->text()) {
        statusLabel->setStyleSheet("color: red;");
        statusLabel->setText(qtTrId("lc-changepin-mismatch"));
    }

    okButton->setEnabled(valid);
}

bool ChangePinDlg::isValidPinLength(const QString &pin) const
{
    return pin.length() >= 4 && pin.length() <= 8;
}

void ChangePinDlg::addToggleVisibilityAction(QLineEdit *edit)
{
    auto hiddenIcon = QIcon::fromTheme("view-hidden");
    auto visibleIcon = QIcon::fromTheme("view-visible");

    auto *action = edit->addAction(hiddenIcon, QLineEdit::TrailingPosition);
    action->setToolTip(qtTrId("lc-changepin-show-hide"));
    connect(action, &QAction::triggered, this, [edit, action, hiddenIcon, visibleIcon]() {
        if (edit->echoMode() == QLineEdit::Password) {
            edit->setEchoMode(QLineEdit::Normal);
            action->setIcon(visibleIcon);
        } else {
            edit->setEchoMode(QLineEdit::Password);
            action->setIcon(hiddenIcon);
        }
    });
}
