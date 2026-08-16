// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "aboutdialog.h"
#include "config.h"

#include <QComboBox>
#include <QDesktopServices>
#include "utils/buttonbox.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QFrame>
#include <QScrollArea>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(const QString& agentVer, QWidget* parent) : QDialog(parent), agentVersion(agentVer)
{
    // Window title is set by retranslateUi() at the end of construction
    // (called from line 203 below). Keeping a single source of truth
    // ensures the dialog always uses the current language.
    setFixedSize(520, 560);

    auto* mainLayout = new QVBoxLayout(this);

    tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs);

    // ── About tab ──────────────────────────────────────────────
    auto* aboutTab = new QWidget(this);
    auto* aboutLayout = new QVBoxLayout(aboutTab);
    aboutLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    aboutLayout->setSpacing(4);
    aboutLayout->setContentsMargins(24, 16, 24, 16);

    // App icon
    iconLabel = new QLabel(aboutTab);
    QPixmap icon(QStringLiteral(":/images/smartcard-id-512.png"));
    iconLabel->setPixmap(icon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(iconLabel);

    // App name
    appNameLabel = new QLabel(aboutTab);
    appNameLabel->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(appNameLabel);

    // Version
    versionLabel = new QLabel(aboutTab);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    aboutLayout->addWidget(versionLabel);

    aboutLayout->addSpacing(6);

    // Description
    descriptionLabel = new QLabel(aboutTab);
    descriptionLabel->setAlignment(Qt::AlignCenter);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    aboutLayout->addWidget(descriptionLabel);

    // Copyright
    copyrightLabel = new QLabel(aboutTab);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    aboutLayout->addWidget(copyrightLabel);

    // Components
    componentsLabel = new QLabel(aboutTab);
    componentsLabel->setAlignment(Qt::AlignCenter);
    componentsLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
    aboutLayout->addWidget(componentsLabel);

    aboutLayout->addSpacing(4);

    // Links
    linksLabel = new QLabel(aboutTab);
    linksLabel->setAlignment(Qt::AlignCenter);
    linksLabel->setOpenExternalLinks(true);
    linksLabel->setStyleSheet(QStringLiteral("font-size: 11px;"));
    aboutLayout->addWidget(linksLabel);

    // Separator
    auto* separator = new QFrame(aboutTab);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    aboutLayout->addWidget(separator);

    // Motivation text
    motivationLabel = new QLabel(aboutTab);
    motivationLabel->setAlignment(Qt::AlignCenter);
    motivationLabel->setWordWrap(true);
    motivationLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    aboutLayout->addWidget(motivationLabel);

    aboutLayout->addSpacing(4);

    // Donate button
    donateButton = new QPushButton(aboutTab);
    donateButton->setCursor(Qt::PointingHandCursor);
    donateButton->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #d63384; color: white; border: none; "
                       "border-radius: 6px; padding: 8px 24px; font-size: 13px; font-weight: bold; }"
                       "QPushButton:hover { background-color: #e74c8b; }"));
    donateButton->setFixedWidth(160);
    aboutLayout->addWidget(donateButton, 0, Qt::AlignCenter);

    connect(donateButton, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://librescrs.github.io/donate"))); });

    aboutLayout->addStretch();
    tabs->addTab(aboutTab, QString());

    // ── Credits tab ────────────────────────────────────────────
    auto* creditsTab = new QWidget(this);
    auto* creditsLayout = new QVBoxLayout(creditsTab);
    creditsLayout->setContentsMargins(24, 16, 24, 16);

    authorsHeading = new QLabel(creditsTab);
    authorsHeading->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    creditsLayout->addWidget(authorsHeading);

    creditsLayout->addSpacing(8);

    // Author card
    auto* authorCard = new QFrame(creditsTab);
    authorCard->setFrameShape(QFrame::StyledPanel);
    authorCard->setStyleSheet(
        QStringLiteral("QFrame { border: 1px solid palette(mid); border-radius: 8px; padding: 12px; }"));
    auto* authorLayout = new QVBoxLayout(authorCard);
    authorLayout->setSpacing(2);

    authorNameLabel = new QLabel(QStringLiteral("hirashix0"), authorCard);
    authorNameLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; border: none;"));
    authorLayout->addWidget(authorNameLabel);

    authorRoleLabel = new QLabel(authorCard);
    authorRoleLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px; border: none;"));
    authorLayout->addWidget(authorRoleLabel);

    authorEmailLabel =
        new QLabel(QStringLiteral("<a href=\"mailto:hirashix0@proton.me\">hirashix0@proton.me</a>"), authorCard);
    authorEmailLabel->setOpenExternalLinks(true);
    authorEmailLabel->setStyleSheet(QStringLiteral("font-size: 11px; border: none;"));
    authorLayout->addWidget(authorEmailLabel);

    creditsLayout->addWidget(authorCard);
    creditsLayout->addStretch();
    tabs->addTab(creditsTab, QString());

    // ── License tab ────────────────────────────────────────────
    auto* licenseTab = new QWidget(this);
    auto* licenseLayout = new QVBoxLayout(licenseTab);
    licenseLayout->setContentsMargins(16, 12, 16, 12);
    licenseLayout->setSpacing(6);

    // The per-component attribution lines live in a capped scroll area so
    // the (long) list never starves the license-text viewer below it.
    auto* attribContainer = new QWidget(licenseTab);
    auto* attribLayout = new QVBoxLayout(attribContainer);
    attribLayout->setContentsMargins(0, 0, 0, 0);
    attribLayout->setSpacing(6);

    licenseLibreCelikLabel = new QLabel(attribContainer);
    licenseLibreCelikLabel->setWordWrap(true);
    licenseLibreCelikLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseLibreCelikLabel);

    licenseAgentLabel = new QLabel(attribContainer);
    licenseAgentLabel->setWordWrap(true);
    licenseAgentLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseAgentLabel);

    licenseOpenSslLabel = new QLabel(attribContainer);
    licenseOpenSslLabel->setWordWrap(true);
    licenseOpenSslLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseOpenSslLabel);

    licenseQtLabel = new QLabel(attribContainer);
    licenseQtLabel->setWordWrap(true);
    licenseQtLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseQtLabel);

    licenseCurlLabel = new QLabel(attribContainer);
    licenseCurlLabel->setWordWrap(true);
    licenseCurlLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseCurlLabel);

    licenseLibXml2Label = new QLabel(attribContainer);
    licenseLibXml2Label->setWordWrap(true);
    licenseLibXml2Label->setStyleSheet(QStringLiteral("font-size: 12px;"));
    attribLayout->addWidget(licenseLibXml2Label);

    attribLayout->addSpacing(6);

    sourceOfferLabel = new QLabel(attribContainer);
    sourceOfferLabel->setWordWrap(true);
    sourceOfferLabel->setOpenExternalLinks(true);
    sourceOfferLabel->setTextFormat(Qt::RichText);
    sourceOfferLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    attribLayout->addWidget(sourceOfferLabel);
    attribLayout->addStretch(1);

    auto* attribScroll = new QScrollArea(licenseTab);
    attribScroll->setWidget(attribContainer);
    attribScroll->setWidgetResizable(true);
    attribScroll->setFrameShape(QFrame::NoFrame);
    attribScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    attribScroll->setMaximumHeight(140);
    licenseLayout->addWidget(attribScroll);

    licenseLayout->addSpacing(6);

    licenseCombo = new QComboBox(licenseTab);
    licenseCombo->addItem(QStringLiteral("GPL-3.0-or-later"), QStringLiteral(":/licenses/gpl-3.0.txt"));
    licenseCombo->addItem(QStringLiteral("LGPL-2.1-or-later"), QStringLiteral(":/licenses/lgpl-2.1.txt"));
    licenseCombo->addItem(QStringLiteral("Apache-2.0"), QStringLiteral(":/licenses/apache-2.0.txt"));
    licenseCombo->addItem(QStringLiteral("Qt 6 LGPL-3.0"), QStringLiteral(":/licenses/lgpl-3.0.txt"));
    licenseCombo->addItem(QStringLiteral("curl"), QStringLiteral(":/licenses/curl.txt"));
    licenseCombo->addItem(QStringLiteral("libxml2 MIT"), QStringLiteral(":/licenses/libxml2.txt"));
    // The "full notices" entry is translatable; its label is set in
    // retranslateUi() by index so it follows runtime language switches.
    fullNoticesIndex = licenseCombo->count();
    licenseCombo->addItem(QString(), QStringLiteral(":/licenses/THIRD-PARTY-LICENSES.txt"));
    licenseLayout->addWidget(licenseCombo);

    licenseBrowser = new QTextBrowser(licenseTab);
    licenseBrowser->setOpenExternalLinks(true);
    licenseBrowser->setStyleSheet(QStringLiteral("font-size: 10px;"));
    licenseLayout->addWidget(licenseBrowser, 1);

    connect(licenseCombo, &QComboBox::currentIndexChanged, this, &AboutDialog::loadLicense);

    tabs->addTab(licenseTab, QString());

    // ── Button box ─────────────────────────────────────────────
    auto* buttonBox = new librecelik::ButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Initial state
    retranslateUi();
    loadLicense(0);
}

void AboutDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void AboutDialog::retranslateUi()
{
    setWindowTitle(qtTrId("lc-about-title"));

    // Tab titles
    tabs->setTabText(0, qtTrId("lc-about-tab-about"));
    tabs->setTabText(1, qtTrId("lc-about-tab-credits"));
    tabs->setTabText(2, qtTrId("lc-about-tab-license"));

    // About tab
    appNameLabel->setText(QStringLiteral("<b style='font-size:18px;'>%1</b>").arg(qtTrId("lc-about-app-name")));
    versionLabel->setText(qtTrId("lc-about-version").arg(QLatin1String(LIBRECELIK_VERSION)));
    descriptionLabel->setText(qtTrId("lc-about-description"));
    copyrightLabel->setText(qtTrId("lc-about-copyright"));

    // The agent is a separate program with its own release cadence, so its
    // version belongs next to Qt's: both are things this process talks to
    // rather than things it is.
    const QString versionOrDash = agentVersion.isEmpty() ? QStringLiteral("—") : agentVersion;
    QString components = QStringLiteral("LibreSCRS Agent %1 · Qt %2").arg(versionOrDash, QLatin1String(qVersion()));
    componentsLabel->setText(components);

    linksLabel->setText(QStringLiteral("<a href=\"https://github.com/LibreSCRS/LibreCelik\">%1</a>"
                                       " · <a href=\"https://librescrs.github.io\">%2</a>")
                            .arg(qtTrId("lc-about-link-github"), qtTrId("lc-about-link-website")));

    motivationLabel->setText(qtTrId("lc-about-donate-motivation"));
    donateButton->setText(qtTrId("lc-about-donate-button"));

    // Credits tab
    authorsHeading->setText(qtTrId("lc-about-credits-authors"));
    authorRoleLabel->setText(qtTrId("lc-about-credits-role-maintainer"));

    // License tab
    licenseLibreCelikLabel->setText(QStringLiteral("<b>LibreCelik</b> — %1").arg(qtTrId("lc-about-license-gpl")));
    licenseAgentLabel->setText(QStringLiteral("<b>LibreAgent ClientQt</b> — %1").arg(qtTrId("lc-about-license-lgpl")));
    licenseOpenSslLabel->setText(QStringLiteral("<b>OpenSSL</b> — %1").arg(qtTrId("lc-about-license-apache")));
    licenseQtLabel->setText(QStringLiteral("<b>Qt 6</b> — %1").arg(qtTrId("lc-about-license-qt")));
    licenseCurlLabel->setText(QStringLiteral("<b>curl</b> — %1").arg(qtTrId("lc-about-license-curl")));
    licenseLibXml2Label->setText(QStringLiteral("<b>libxml2</b> — %1").arg(qtTrId("lc-about-license-libxml2")));

    if (fullNoticesIndex >= 0)
        licenseCombo->setItemText(fullNoticesIndex, qtTrId("lc-about-full-notices"));

    sourceOfferLabel->setText(qtTrId("lc-about-source-offer"));
}

void AboutDialog::loadLicense(int index)
{
    QString path = licenseCombo->itemData(index).toString();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        licenseBrowser->setPlainText(QString::fromUtf8(file.readAll()));
}
