// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "changepindlg.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QIcon>
#include <QRegularExpressionValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

// Programmatic fallback used when QIcon::fromTheme returns nothing (macOS).
static QIcon createEyeIcon(bool open)
{
    QPixmap px(16, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor col(90, 90, 90);
    p.setPen(QPen(col, 1.2));
    p.setBrush(Qt::NoBrush);
    // Outer eye ellipse (horizontal lens shape)
    p.drawEllipse(QRectF(0.5, 3.5, 15.0, 9.0));
    // Pupil
    p.setBrush(col);
    p.drawEllipse(QRectF(5.5, 5.5, 5.0, 5.0));
    if (!open) {
        // Diagonal slash: eye-hidden / password concealed
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(col, 1.5));
        p.drawLine(QPointF(2.5, 13.5), QPointF(13.5, 2.5));
    }
    return QIcon(px);
}

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
    addToggleVisibilityAction(currentPinEdit);
    layout->addWidget(currentPinEdit);

    newPinEdit = new QLineEdit(this);
    newPinEdit->setEchoMode(QLineEdit::Password);
    newPinEdit->setMaxLength(pinMaxLength);
    newPinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), this));
    newPinEdit->setPlaceholderText(qtTrId("lc-changepin-new"));
    addToggleVisibilityAction(newPinEdit);
    layout->addWidget(newPinEdit);

    confirmPinEdit = new QLineEdit(this);
    confirmPinEdit->setEchoMode(QLineEdit::Password);
    confirmPinEdit->setMaxLength(pinMaxLength);
    confirmPinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), this));
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

void ChangePinDlg::addToggleVisibilityAction(QLineEdit* edit)
{
    // QIcon::fromTheme returns a null icon on macOS (no FreeDesktop theme);
    // fall back to programmatically drawn icons so the button is always visible.
    auto hiddenIcon = QIcon::fromTheme("view-hidden", createEyeIcon(false));
    auto visibleIcon = QIcon::fromTheme("view-visible", createEyeIcon(true));

    auto* action = edit->addAction(hiddenIcon, QLineEdit::TrailingPosition);
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
