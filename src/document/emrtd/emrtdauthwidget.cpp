// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdauthwidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

EMRTDAuthWidget::EMRTDAuthWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    // CAN input section
    auto* canSection = new QWidget(this);
    auto* canLayout = new QVBoxLayout(canSection);
    canLayout->setContentsMargins(0, 0, 0, 0);
    canLayout->setAlignment(Qt::AlignCenter);

    auto* titleLabel = new QLabel(qtTrId("lc-emrtd-auth-can-title"), canSection);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px;");
    canLayout->addWidget(titleLabel);

    auto* hintLabel = new QLabel(qtTrId("lc-emrtd-auth-can-desc"), canSection);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #888; font-size: 10px;");
    canLayout->addWidget(hintLabel);

    canEdit = new QLineEdit(canSection);
    canEdit->setMaxLength(6);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    canEdit->setPlaceholderText("000000");
    canEdit->setStyleSheet("font-family: monospace; font-size: 20px; letter-spacing: 10px;"
                           "padding: 8px; text-align: center;");
    canEdit->setAlignment(Qt::AlignCenter);
    canEdit->setMaximumWidth(240);
    canLayout->addWidget(canEdit, 0, Qt::AlignCenter);

    statusLabel = new QLabel(canSection);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setVisible(false);
    canLayout->addWidget(statusLabel);

    authButton = new QPushButton(qtTrId("lc-emrtd-authenticate"), canSection);
    authButton->setEnabled(false);
    authButton->setDefault(true);
    canLayout->addWidget(authButton, 0, Qt::AlignCenter);

    layout->addWidget(canSection);

    // Progress bar — must exist for isSpinner() detection in addNewReader().
    // isSpinner() checks findChild<QProgressBar*>() != nullptr.
    // This is intentional: when PACE succeeds, cardGroupReady sees isSpinner()==true
    // and replaces this widget with the streaming card data widget.
    auto* spinnerSection = new QWidget(this);
    auto* spinnerLayout = new QVBoxLayout(spinnerSection);
    spinnerLayout->setContentsMargins(0, 16, 0, 0);
    spinnerLayout->setAlignment(Qt::AlignCenter);

    auto* bar = new QProgressBar(spinnerSection);
    bar->setRange(0, 0); // indeterminate
    bar->setMaximumWidth(200);
    bar->setTextVisible(false);
    spinnerLayout->addWidget(bar, 0, Qt::AlignCenter);

    auto* waitLabel = new QLabel(qtTrId("lc-emrtd-authenticating"), spinnerSection);
    waitLabel->setAlignment(Qt::AlignCenter);
    waitLabel->setStyleSheet("color: #888; font-size: 11px;");
    spinnerLayout->addWidget(waitLabel);

    layout->addWidget(spinnerSection);

    // Connections
    connect(authButton, &QPushButton::clicked, this, &EMRTDAuthWidget::onAuthenticateClicked);
    connect(canEdit, &QLineEdit::textChanged, this, &EMRTDAuthWidget::validateForm);
    connect(canEdit, &QLineEdit::returnPressed, this, &EMRTDAuthWidget::onAuthenticateClicked);

    canEdit->setFocus();
}

void EMRTDAuthWidget::onAuthenticateClicked()
{
    if (!authButton->isEnabled())
        return;
    authButton->setEnabled(false);
    statusLabel->setStyleSheet("color: #E6873C;");
    statusLabel->setText(qtTrId("lc-emrtd-authenticating"));
    statusLabel->setVisible(true);

    QMap<QString, QString> credentials;
    credentials["can"] = canEdit->text();
    emit credentialsEntered(credentials);
}

void EMRTDAuthWidget::onAuthFailed(const QString& errorMessage)
{
    statusLabel->setStyleSheet("color: #CC3333;");
    statusLabel->setText(errorMessage);
    statusLabel->setVisible(true);
    authButton->setEnabled(true);
}

void EMRTDAuthWidget::validateForm()
{
    authButton->setEnabled(canEdit->text().length() == 6);
}
