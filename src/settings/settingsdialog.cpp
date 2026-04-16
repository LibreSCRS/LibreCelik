// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "settingsdialog.h"

#include "settings/settingskeys.h"
#include "settings/tlitemdelegate.h"
#include "signing/defaults.h"
#include "signing/tsaitemdelegate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
bool isValidServiceUrl(const QString& url)
{
    QUrl parsed(url);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return false;
    const QString scheme = parsed.scheme();
    return scheme == QStringLiteral("https") || scheme == QStringLiteral("http");
}
} // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(qtTrId("lc-settings-title"));
    setMinimumSize(500, 400);
    resize(600, 450);

    auto* layout = new QVBoxLayout(this);

    tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    // --- General tab ---
    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QFormLayout(generalTab);

    languageCombo = new QComboBox(generalTab);
    languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageCombo->addItem(QStringLiteral("Српски"), QStringLiteral("sr_RS"));
    languageLabel = new QLabel(qtTrId("lc-settings-language"), generalTab);
    generalLayout->addRow(languageLabel, languageCombo);

    connect(languageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        // Live preview — emit immediately but don't persist until OK is clicked
        emit languageChanged(languageCombo->currentData().toString());
    });

    tabs->addTab(generalTab, qtTrId("lc-settings-tab-general"));

    // --- Signing tab ---
    auto* signingTab = new QWidget(this);
    auto* signingLayout = new QVBoxLayout(signingTab);

    auto* signingForm = new QFormLayout;

    defaultLevelCombo = new QComboBox(signingTab);
    defaultLevelCombo->addItem(qtTrId("lc-sign-level-bb"), QStringLiteral("B_B"));
    defaultLevelCombo->addItem(qtTrId("lc-sign-level-bt"), QStringLiteral("B_T"));
    defaultLevelCombo->addItem(qtTrId("lc-sign-level-blt"), QStringLiteral("B_LT"));
    defaultLevelCombo->addItem(qtTrId("lc-sign-level-blta"), QStringLiteral("B_LTA"));
    defaultLevelLabel = new QLabel(qtTrId("lc-settings-default-level"), signingTab);
    signingForm->addRow(defaultLevelLabel, defaultLevelCombo);

    auto* outputRow = new QHBoxLayout;
    defaultOutputFolder = new QLineEdit(signingTab);
    defaultOutputFolder->setReadOnly(true);
    defaultOutputFolder->setPlaceholderText(qtTrId("lc-settings-output-placeholder"));
    browseOutputBtn = new QPushButton(qtTrId("lc-sign-change-folder"), signingTab);
    outputRow->addWidget(defaultOutputFolder, 1);
    outputRow->addWidget(browseOutputBtn);
    defaultOutputLabel = new QLabel(qtTrId("lc-settings-default-output"), signingTab);
    signingForm->addRow(defaultOutputLabel, outputRow);

    signingLayout->addLayout(signingForm);

    // TSA servers
    tsaServersLabel = new QLabel(qtTrId("lc-settings-tsa-servers"), signingTab);
    signingLayout->addWidget(tsaServersLabel);
    tsaList = new QListWidget(signingTab);
    tsaList->setMinimumHeight(100);
    signingLayout->addWidget(tsaList);
    populateTsaList();

    auto* tsaDelegate = new TsaItemDelegate(tsaList, this);
    tsaList->setItemDelegate(tsaDelegate);
    connect(tsaDelegate, &TsaItemDelegate::addRequested, this, &SettingsDialog::onTsaAddRequested);
    connect(tsaDelegate, &TsaItemDelegate::removeRequested, this, [this](int row) {
        if (row >= 0 && row < tsaList->count() &&
            tsaList->item(row)->data(Qt::UserRole).toString() == QStringLiteral("custom")) {
            delete tsaList->takeItem(row);
        }
    });

    signingLayout->addStretch();

    tabs->addTab(signingTab, qtTrId("lc-settings-tab-signing"));

    connect(browseOutputBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, qtTrId("lc-sign-select-output-folder"),
                                                        defaultOutputFolder->text());
        if (!dir.isEmpty())
            defaultOutputFolder->setText(dir);
    });

    // --- Trust tab ---
    auto* trustTab = new QWidget(this);
    auto* trustLayout = new QVBoxLayout(trustTab);

    tlServersLabel = new QLabel(qtTrId("lc-settings-tl-servers"), trustTab);
    trustLayout->addWidget(tlServersLabel);
    tlList = new QListWidget(trustTab);
    tlList->setMinimumHeight(120);
    trustLayout->addWidget(tlList);
    populateTlList();

    auto* tlDelegate = new TlItemDelegate(tlList, this);
    tlList->setItemDelegate(tlDelegate);
    connect(tlDelegate, &TlItemDelegate::addRequested, this, &SettingsDialog::onTlAddRequested);
    connect(tlDelegate, &TlItemDelegate::removeRequested, this, [this](int row) {
        if (row >= 0 && row < tlList->count() &&
            tlList->item(row)->data(TlItemDelegate::TypeRole).toString() == QStringLiteral("custom")) {
            delete tlList->takeItem(row);
        }
    });

    auto* cacheForm = new QFormLayout;
    auto* cacheRow = new QHBoxLayout;
    cacheDir = new QLineEdit(trustTab);
    cacheDir->setReadOnly(true);
    browseCacheBtn = new QPushButton(qtTrId("lc-sign-change-folder"), trustTab);
    cacheRow->addWidget(cacheDir, 1);
    cacheRow->addWidget(browseCacheBtn);
    cacheDirLabel = new QLabel(qtTrId("lc-settings-cache-dir"), trustTab);
    cacheForm->addRow(cacheDirLabel, cacheRow);
    trustLayout->addLayout(cacheForm);

    trustLayout->addStretch();

    tabs->addTab(trustTab, qtTrId("lc-settings-tab-trust"));

    connect(browseCacheBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, qtTrId("lc-settings-cache-dir"), cacheDir->text());
        if (!dir.isEmpty())
            cacheDir->setText(dir);
    });

    // --- Button box ---
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        // Restore the original language if the user changed it during live preview
        QString currentLocale = languageCombo->currentData().toString();
        if (currentLocale != originalLocale)
            emit languageChanged(originalLocale);
        reject();
    });

    loadSettings();
}

void SettingsDialog::loadSettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    QString locale = settings.value(settings::kLanguage, QString()).toString();
    originalLocale = locale;
    int langIdx = languageCombo->findData(locale);
    if (langIdx >= 0)
        languageCombo->setCurrentIndex(langIdx);

    // Signing
    int levelIdx =
        defaultLevelCombo->findData(settings.value(settings::kSigningDefaultLevel, QStringLiteral("B_B")).toString());
    if (levelIdx >= 0)
        defaultLevelCombo->setCurrentIndex(levelIdx);
    defaultOutputFolder->setText(settings.value(settings::kSigningDefaultOutputFolder).toString());

    // Trust
    QString defaultCache =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/librescrs/tsl");
    cacheDir->setText(settings.value(settings::kTslCacheDir, defaultCache).toString());
}

void SettingsDialog::saveSettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    // Language (persisted on OK, live preview was immediate)
    settings.setValue(settings::kLanguage, languageCombo->currentData().toString());

    // Signing
    settings.setValue(settings::kSigningDefaultLevel, defaultLevelCombo->currentData().toString());
    settings.setValue(settings::kSigningDefaultOutputFolder, defaultOutputFolder->text());

    // TSA servers (save only custom URLs)
    QStringList tsaUrls;
    for (int i = 0; i < tsaList->count(); ++i) {
        if (tsaList->item(i)->data(Qt::UserRole).toString() == QStringLiteral("custom"))
            tsaUrls.append(tsaList->item(i)->text());
    }
    settings.setValue(settings::kSigningTsaUrls, tsaUrls);

    // Trust
    QJsonArray tlEntries;
    for (int i = 0; i < tlList->count(); ++i) {
        auto* item = tlList->item(i);
        if (item->data(TlItemDelegate::TypeRole).toString() == QStringLiteral("add"))
            continue;
        QJsonObject obj;
        obj[QStringLiteral("url")] = item->text();
        obj[QStringLiteral("lotl")] = item->data(TlItemDelegate::IsLotlRole).toBool();
        obj[QStringLiteral("eager")] = item->data(TlItemDelegate::EagerRole).toBool();
        tlEntries.append(obj);
    }
    settings.setValue(settings::kTslEntries, QJsonDocument(tlEntries).toJson(QJsonDocument::Compact));
    settings.setValue(settings::kTslCacheDir, cacheDir->text());
}

void SettingsDialog::populateTsaList()
{
    tsaList->clear();

    for (const QString& url : signing::defaultTsaUrls()) {
        auto* item = new QListWidgetItem(url, tsaList);
        item->setData(Qt::UserRole, QStringLiteral("default"));
    }

    QSettings settings(settings::kOrganization, settings::kApplication);
    QStringList custom = settings.value(settings::kSigningTsaUrls).toStringList();
    for (const QString& rawUrl : custom) {
        const QString url = rawUrl.trimmed();
        // Re-validate on load: a corrupted/manually-edited settings file
        // could otherwise sneak invalid URLs (e.g. file://, javascript:) past
        // the dialog's add-time check.
        if (url.isEmpty() || !isValidServiceUrl(url))
            continue;
        auto* item = new QListWidgetItem(url, tsaList);
        item->setData(Qt::UserRole, QStringLiteral("custom"));
    }

    // Add row
    auto* addItem = new QListWidgetItem(qtTrId("lc-sign-tsa-add-item"), tsaList);
    addItem->setData(Qt::UserRole, QStringLiteral("add"));
}

void SettingsDialog::onTsaAddRequested()
{
    QString url = QInputDialog::getText(this, qtTrId("lc-sign-tsa-add-title"), qtTrId("lc-sign-tsa-add-prompt"));
    url = url.trimmed();
    if (url.isEmpty())
        return;
    if (!url.startsWith(QStringLiteral("https://")) && !url.startsWith(QStringLiteral("http://")))
        url.prepend(QStringLiteral("https://"));
    if (!isValidServiceUrl(url)) {
        QMessageBox::warning(this, qtTrId("lc-settings-invalid-url-title"), qtTrId("lc-settings-invalid-url-msg"));
        return;
    }
    for (int i = 0; i < tsaList->count(); ++i) {
        if (tsaList->item(i)->text() == url)
            return;
    }
    int addRow = tsaList->count() - 1;
    auto* item = new QListWidgetItem(url);
    item->setData(Qt::UserRole, QStringLiteral("custom"));
    tsaList->insertItem(addRow, item);
}

void SettingsDialog::populateTlList()
{
    tlList->clear();

    const auto& defaults = signing::defaultTrustedLists();

    QSettings settings(settings::kOrganization, settings::kApplication);
    QJsonArray entries = QJsonDocument::fromJson(settings.value(settings::kTslEntries).toByteArray()).array();

    // Fallback: try old QVariant-based format if JSON parsing returned nothing
    if (entries.isEmpty())
        entries = settings.value(settings::kTslEntries).toJsonArray();

    if (entries.isEmpty()) {
        for (const auto& d : defaults) {
            auto* item = new QListWidgetItem(d.url, tlList);
            item->setData(TlItemDelegate::TypeRole, QStringLiteral("default"));
            item->setData(TlItemDelegate::IsLotlRole, d.lotl);
            item->setData(TlItemDelegate::EagerRole, d.eager);
        }
    } else {
        for (const auto& entry : entries) {
            auto obj = entry.toObject();
            const QString url = obj[QStringLiteral("url")].toString().trimmed();
            // Re-validate on load (see populateTsaList for rationale).
            if (url.isEmpty() || !isValidServiceUrl(url))
                continue;
            bool lotl = obj[QStringLiteral("lotl")].toBool(false);
            bool eager = obj[QStringLiteral("eager")].toBool(true);

            bool isDefault = false;
            for (const auto& d : defaults) {
                if (d.url == url) {
                    isDefault = true;
                    break;
                }
            }

            auto* item = new QListWidgetItem(url, tlList);
            item->setData(TlItemDelegate::TypeRole, isDefault ? QStringLiteral("default") : QStringLiteral("custom"));
            item->setData(TlItemDelegate::IsLotlRole, lotl);
            item->setData(TlItemDelegate::EagerRole, eager);
        }
    }

    auto* addItem = new QListWidgetItem(qtTrId("lc-settings-tl-add-item"), tlList);
    addItem->setData(TlItemDelegate::TypeRole, QStringLiteral("add"));
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(qtTrId("lc-settings-title"));
    tabs->setTabText(0, qtTrId("lc-settings-tab-general"));
    tabs->setTabText(1, qtTrId("lc-settings-tab-signing"));
    tabs->setTabText(2, qtTrId("lc-settings-tab-trust"));

    languageLabel->setText(qtTrId("lc-settings-language"));
    defaultLevelLabel->setText(qtTrId("lc-settings-default-level"));
    defaultOutputLabel->setText(qtTrId("lc-settings-default-output"));
    defaultOutputFolder->setPlaceholderText(qtTrId("lc-settings-output-placeholder"));
    browseOutputBtn->setText(qtTrId("lc-sign-change-folder"));
    tsaServersLabel->setText(qtTrId("lc-settings-tsa-servers"));
    tlServersLabel->setText(qtTrId("lc-settings-tl-servers"));
    cacheDirLabel->setText(qtTrId("lc-settings-cache-dir"));
    browseCacheBtn->setText(qtTrId("lc-sign-change-folder"));

    int levelIdx = defaultLevelCombo->currentIndex();
    defaultLevelCombo->setItemText(0, qtTrId("lc-sign-level-bb"));
    defaultLevelCombo->setItemText(1, qtTrId("lc-sign-level-bt"));
    defaultLevelCombo->setItemText(2, qtTrId("lc-sign-level-blt"));
    defaultLevelCombo->setItemText(3, qtTrId("lc-sign-level-blta"));
    defaultLevelCombo->setCurrentIndex(levelIdx);

    populateTsaList();
    populateTlList();
}

void SettingsDialog::onTlAddRequested()
{
    QDialog dlg(this);
    dlg.setWindowTitle(qtTrId("lc-settings-tl-add-title"));
    dlg.setMinimumWidth(400);
    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout;

    auto* urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText(QStringLiteral("https://..."));
    form->addRow(QStringLiteral("URL:"), urlEdit);

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(QStringLiteral("TL"), false);
    typeCombo->addItem(QStringLiteral("LOTL"), true);
    form->addRow(qtTrId("lc-settings-tl-type"), typeCombo);

    auto* loadCombo = new QComboBox(&dlg);
    loadCombo->addItem(QStringLiteral("Eager"), true);
    loadCombo->addItem(QStringLiteral("Lazy"), false);
    form->addRow(qtTrId("lc-settings-tl-loading"), loadCombo);

    layout->addLayout(form);
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString url = urlEdit->text().trimmed();
    if (url.isEmpty())
        return;
    if (!url.startsWith(QStringLiteral("https://")) && !url.startsWith(QStringLiteral("http://")))
        url.prepend(QStringLiteral("https://"));
    if (!isValidServiceUrl(url)) {
        QMessageBox::warning(this, qtTrId("lc-settings-invalid-url-title"), qtTrId("lc-settings-invalid-url-msg"));
        return;
    }

    int addRow = tlList->count() - 1;
    auto* item = new QListWidgetItem(url);
    item->setData(TlItemDelegate::TypeRole, QStringLiteral("custom"));
    item->setData(TlItemDelegate::IsLotlRole, typeCombo->currentData().toBool());
    item->setData(TlItemDelegate::EagerRole, loadCombo->currentData().toBool());
    tlList->insertItem(addRow, item);
}
