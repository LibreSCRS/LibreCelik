// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "emrtdauthwidget.h"

#include <QDateEdit>
#include <QEvent>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QShowEvent>
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
    canHint->setObjectName("hint");
    canHint->setAlignment(Qt::AlignCenter);
    canHint->setStyleSheet(
        QString("color: %1; font-size: 10px;").arg(palette().color(QPalette::PlaceholderText).name()));
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
    mrzHint->setObjectName("hint");
    mrzHint->setAlignment(Qt::AlignCenter);
    mrzHint->setWordWrap(true);
    mrzHint->setStyleSheet(
        QString("color: %1; font-size: 10px;").arg(palette().color(QPalette::PlaceholderText).name()));
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
    waitLabel->setObjectName("hint");
    waitLabel->setAlignment(Qt::AlignCenter);
    waitLabel->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(palette().color(QPalette::PlaceholderText).name()));
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

void EMRTDAuthWidget::showEvent(QShowEvent* event)
{
    // Focus the active tab's primary input once the widget is actually shown.
    // setDefaultTab() runs before the widget is visible, so a setFocus() there
    // does not stick; applying it on show makes the default focus reliable for
    // both tabs (CAN field on the CAN tab, document number on the MRZ tab).
    QWidget::showEvent(event);
    if (tabWidget->currentIndex() == 0)
        canEdit->setFocus();
    else
        docNumberEdit->setFocus();
}

void EMRTDAuthWidget::onAuthenticateClicked()
{
    if (!authButton->isEnabled())
        return;

    // Read CAN/MRZ from QLineEdits BEFORE any visual mutation (status label,
    // button disable, edit clear). The bytes are adopted into Secure::String
    // immediately via the std::string&& adopt-and-cleanse ctor — no
    // intermediate QString copy outlives this function. Mirrors the pattern
    // in `signing/signpage.cpp` (`pinSecure`/`canSecure`) where the secure
    // capture happens before the source widgets are hidden.
    //
    // See `feedback_read_then_hide_secrets.md`: read first, hide second —
    // QLineEdit visibility-state is sometimes a downstream logic gate, but
    // here the widget itself stays alive (the spinner section is a sibling),
    // so the only requirement is that the source storage be evicted from
    // QString CoW heap pages immediately after the secure copy is built.
    EmrtdCredentials credentials;
    if (tabWidget->currentIndex() == 0) {
        credentials.mode = EmrtdCredentials::Mode::CAN;
        credentials.can = LibreSCRS::Secure::String{canEdit->text().toStdString()};
    } else {
        credentials.mode = EmrtdCredentials::Mode::MRZ;
        credentials.mrzDocNumber = LibreSCRS::Secure::String{docNumberEdit->text().toStdString()};
        credentials.mrzDob = LibreSCRS::Secure::String{dobEdit->date().toString("yyMMdd").toStdString()};
        credentials.mrzExpiry = LibreSCRS::Secure::String{expiryEdit->date().toString("yyMMdd").toStdString()};
    }

    // Evict CAN/MRZ from the QLineEdit's QString CoW storage. QLineEdit::clear
    // re-assigns an empty string, which drops the last shared ref to the
    // previous CoW block. The block is then deallocated by the implicit-
    // sharing ref-count drop without explicit cleansing, but the practical
    // window where a memory-dump primitive can recover the digits is closed
    // as soon as the heap page is reused.
    canEdit->clear();
    docNumberEdit->clear();
    // QDateEdits revert to the sentinel date so the displayed yyMMdd is
    // overwritten in the input widget's internal QString storage.
    dobEdit->setDate(kSentinelDate);
    expiryEdit->setDate(kSentinelDate);

    authButton->setEnabled(false);
    statusLabel->setStyleSheet("color: #E6873C;");
    statusLabel->setText(qtTrId("lc-emrtd-authenticating"));
    statusLabel->setVisible(true);

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

void EMRTDAuthWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        tabWidget->setTabText(0, qtTrId("lc-emrtd-auth-can-tab"));
        tabWidget->setTabText(1, qtTrId("lc-emrtd-auth-mrz-tab"));
        authButton->setText(qtTrId("lc-emrtd-authenticate"));
    } else if (event->type() == QEvent::PaletteChange) {
        auto hints = findChildren<QLabel*>("hint");
        QString color = palette().color(QPalette::PlaceholderText).name();
        for (auto* hint : hints) {
            QString ss = hint->styleSheet();
            if (ss.contains("font-size: 11px"))
                hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(color));
            else
                hint->setStyleSheet(QString("color: %1; font-size: 10px;").arg(color));
        }
    }
    QWidget::changeEvent(event);
}
