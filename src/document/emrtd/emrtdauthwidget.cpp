// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdauthwidget.h"

#include <QDateEdit>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
const QDate kSentinelDate(1900, 1, 1);
} // namespace

EMRTDAuthWidget::EMRTDAuthWidget(QWidget* parent)
    : QWidget(parent), docNumberEdit(nullptr), dobEdit(nullptr), expiryEdit(nullptr)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // --- Tab widget ---
    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);
    tabWidget->tabBar()->setExpanding(true);

    // CAN tab
    auto* canPage = new QWidget();
    auto* canLayout = new QVBoxLayout(canPage);
    canLayout->setAlignment(Qt::AlignCenter);

    auto* canTitle = new QLabel(qtTrId("lc-emrtd-auth-can-title"), canPage);
    canTitle->setAlignment(Qt::AlignCenter);
    canTitle->setStyleSheet("font-size: 14px;");
    canLayout->addWidget(canTitle);

    auto* canHint = new QLabel(qtTrId("lc-emrtd-auth-can-desc"), canPage);
    canHint->setAlignment(Qt::AlignCenter);
    canHint->setStyleSheet("color: #888; font-size: 10px;");
    canLayout->addWidget(canHint);

    canEdit = new QLineEdit(canPage);
    canEdit->setMaxLength(6);
    canEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{6}"), this));
    canEdit->setPlaceholderText("000000");
    canEdit->setStyleSheet("font-family: monospace; font-size: 20px; letter-spacing: 10px;"
                           "padding: 8px; text-align: center;");
    canEdit->setAlignment(Qt::AlignCenter);
    canEdit->setMaximumWidth(240);
    canLayout->addWidget(canEdit, 0, Qt::AlignCenter);

    tabWidget->addTab(canPage, qtTrId("lc-emrtd-auth-can-tab"));

    // MRZ tab
    auto* mrzPage = new QWidget();
    auto* mrzLayout = new QVBoxLayout(mrzPage);
    mrzLayout->setAlignment(Qt::AlignCenter);

    auto* mrzTitle = new QLabel(qtTrId("lc-emrtd-auth-mrz-title"), mrzPage);
    mrzTitle->setAlignment(Qt::AlignCenter);
    mrzTitle->setStyleSheet("font-size: 14px;");
    mrzLayout->addWidget(mrzTitle);

    auto* mrzHint = new QLabel(qtTrId("lc-emrtd-auth-mrz-desc"), mrzPage);
    mrzHint->setAlignment(Qt::AlignCenter);
    mrzHint->setWordWrap(true);
    mrzHint->setStyleSheet("color: #888; font-size: 10px;");
    mrzLayout->addWidget(mrzHint);

    auto* formWidget = new QWidget(mrzPage);
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    docNumberEdit = new QLineEdit(formWidget);
    docNumberEdit->setMaxLength(9);
    docNumberEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9]{0,9}"), this));
    docNumberEdit->setPlaceholderText("AB1234567");
    connect(docNumberEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        QString upper = text.toUpper();
        if (upper != text) {
            docNumberEdit->setText(upper);
            return;
        }
        validateForm();
    });
    formLayout->addRow(qtTrId("lc-emrtd-auth-mrz-docnum"), docNumberEdit);

    dobEdit = new QDateEdit(kSentinelDate, formWidget);
    dobEdit->setCalendarPopup(true);
    dobEdit->setDisplayFormat("dd.MM.yyyy");
    dobEdit->setMaximumDate(QDate::currentDate());
    connect(dobEdit, &QDateEdit::dateChanged, this, &EMRTDAuthWidget::validateForm);
    formLayout->addRow(qtTrId("lc-emrtd-auth-mrz-dob"), dobEdit);

    expiryEdit = new QDateEdit(kSentinelDate, formWidget);
    expiryEdit->setCalendarPopup(true);
    expiryEdit->setDisplayFormat("dd.MM.yyyy");
    connect(expiryEdit, &QDateEdit::dateChanged, this, &EMRTDAuthWidget::validateForm);
    formLayout->addRow(qtTrId("lc-emrtd-auth-mrz-expiry"), expiryEdit);

    mrzLayout->addWidget(formWidget, 0, Qt::AlignCenter);

    tabWidget->addTab(mrzPage, qtTrId("lc-emrtd-auth-mrz-tab"));

    layout->addWidget(tabWidget);

    // --- Shared elements ---
    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setVisible(false);
    layout->addWidget(statusLabel);

    authButton = new QPushButton(qtTrId("lc-emrtd-authenticate"), this);
    authButton->setEnabled(false);
    authButton->setDefault(true);
    layout->addWidget(authButton, 0, Qt::AlignCenter);

    // Mark this widget as a spinner for isSpinner() detection in addNewReader().
    setProperty("isSpinner", true);

    auto* spinnerSection = new QWidget(this);
    auto* spinnerLayout = new QVBoxLayout(spinnerSection);
    spinnerLayout->setContentsMargins(0, 16, 0, 0);
    spinnerLayout->setAlignment(Qt::AlignCenter);

    auto* bar = new QProgressBar(spinnerSection);
    bar->setRange(0, 0);
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
    connect(docNumberEdit, &QLineEdit::returnPressed, this, &EMRTDAuthWidget::onAuthenticateClicked);
    connect(tabWidget, &QTabWidget::currentChanged, this, [this]() {
        validateForm();
        if (tabWidget->currentIndex() == 0)
            canEdit->setFocus();
        else if (docNumberEdit)
            docNumberEdit->setFocus();
    });

    canEdit->setFocus();
}

void EMRTDAuthWidget::setDefaultTab(bool paceSupported)
{
    tabWidget->setCurrentIndex(paceSupported ? 0 : 1);
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
    if (tabWidget->currentIndex() == 0) {
        credentials["can"] = canEdit->text();
    } else {
        credentials["mrz_doc_number"] = docNumberEdit->text();
        credentials["mrz_dob"] = dobEdit->date().toString("yyMMdd");
        credentials["mrz_expiry"] = expiryEdit->date().toString("yyMMdd");
    }
    emit credentialsEntered(credentials);
}

void EMRTDAuthWidget::onAuthFailed(const QString& errorMessage)
{
    statusLabel->setStyleSheet("color: #CC3333;");
    statusLabel->setText(errorMessage);
    statusLabel->setVisible(true);
    authButton->setEnabled(true);
    validateForm();
}

void EMRTDAuthWidget::validateForm()
{
    if (tabWidget->currentIndex() == 0) {
        authButton->setEnabled(canEdit->hasAcceptableInput());
    } else {
        bool docOk = !docNumberEdit->text().isEmpty();
        bool dobOk = dobEdit->date() != kSentinelDate && dobEdit->date() <= QDate::currentDate();
        bool expiryOk = expiryEdit->date() != kSentinelDate;
        authButton->setEnabled(docOk && dobOk && expiryOk);
    }
}
