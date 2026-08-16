// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "fileselectionpage.h"

#include "defaults.h"
#include "filedropzone.h"
#include "filelistdelegate.h"
#include "settings/settingskeys.h"
#include "tsavalidation.h"

#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

LibreSCRS::AgentClient::SignatureFormat FileSelectionPage::defaultFormatForExtension(const QString& suffix)
{
    const QString s = suffix.toLower();
    if (s == QStringLiteral("pdf"))
        return LibreSCRS::AgentClient::SignatureFormat::PAdES;
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return LibreSCRS::AgentClient::SignatureFormat::XAdES;
    if (s == QStringLiteral("json"))
        return LibreSCRS::AgentClient::SignatureFormat::JAdES;
    return LibreSCRS::AgentClient::SignatureFormat::ASiCe;
}

QList<FileFormatInfo> FileSelectionPage::availableFormatsForExtension(const QString& suffix)
{
    using F = LibreSCRS::AgentClient::SignatureFormat;
    using P = LibreSCRS::AgentClient::Packaging;
    const QString s = suffix.toLower();

    if (s == QStringLiteral("pdf"))
        return {{F::PAdES, P::Enveloped}, {F::ASiCe, P::Enveloped}};
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return {
            {F::XAdES, P::Enveloped},
            {F::XAdES, P::Detached},
            {F::ASiCe, P::Enveloped},
            {F::CAdES, P::Detached},
        };
    if (s == QStringLiteral("json"))
        return {
            {F::JAdES, P::Detached},
            {F::ASiCe, P::Enveloped},
            {F::CAdES, P::Detached},
        };
    return {{F::ASiCe, P::Enveloped}, {F::CAdES, P::Detached}};
}

QString FileSelectionPage::formatDisplayName(LibreSCRS::AgentClient::SignatureFormat format,
                                             LibreSCRS::AgentClient::Packaging packaging)
{
    using F = LibreSCRS::AgentClient::SignatureFormat;
    using P = LibreSCRS::AgentClient::Packaging;
    switch (format) {
    case F::PAdES:
        return QStringLiteral("PAdES");
    case F::CAdES:
        return QStringLiteral("CAdES (.p7s)");
    case F::XAdES:
        return packaging == P::Enveloped ? QStringLiteral("XAdES (enveloped)") : QStringLiteral("XAdES (detached)");
    case F::ASiCe:
        return QStringLiteral("ASiC-E (.asice)");
    case F::JAdES:
        return QStringLiteral("JAdES (.jose)");
    }
    return QStringLiteral("Unknown");
}

FileSelectionPage::FileSelectionPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // Drop zone
    dropZone = new FileDropZone(this);
    layout->addWidget(dropZone);

    // File size limits info (doubles as the selection-cap refusal line)
    limitsLabel = new QLabel(this);
    limitsLabel->setObjectName(QStringLiteral("limitsLabel"));
    limitsLabel->setWordWrap(true);
    auto limitsFont = limitsLabel->font();
    limitsFont.setPointSize(limitsFont.pointSize() - 1);
    limitsLabel->setFont(limitsFont);
    limitsLabel->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(limitsLabel);

    // Announced only against an agent that cannot sign a batch (see
    // setCapabilities); hidden by default because the page assumes the
    // capability until told otherwise.
    sequentialNotice = new QLabel(this);
    sequentialNotice->setObjectName(QStringLiteral("sequentialNotice"));
    sequentialNotice->setWordWrap(true);
    sequentialNotice->setFont(limitsFont);
    sequentialNotice->setVisible(false);
    layout->addWidget(sequentialNotice);

    // File list (Delete key removes selected file)
    fileList = new QListWidget(this);
    fileList->setMinimumHeight(80);
    fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    fileList->setItemDelegate(new FileListDelegate(fileList));
    layout->addWidget(fileList);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, fileList);
    connect(deleteShortcut, &QShortcut::activated, this, &FileSelectionPage::removeSelectedFile);
    connect(fileList, &QListWidget::itemDoubleClicked, this, [this]() { removeSelectedFile(); });

    connect(fileList, &QListWidget::itemClicked, this, [this](QListWidgetItem*) {
        int row = fileList->currentRow();
        showFormatComboForItem(row);
    });

    // Format description label (shown/hidden based on file types)
    formatDesc = new QLabel(this);
    formatDesc->setWordWrap(true);
    formatDesc->setVisible(false);
    layout->addWidget(formatDesc);

    // Signature level (labels filled by retranslateUi() at end of ctor)
    levelLabel = new QLabel(this);
    levelCombo = new QComboBox(this);
    levelCombo->addItem(QString(), QStringLiteral("B_B"));
    levelCombo->addItem(QString(), QStringLiteral("B_T"));
    levelCombo->addItem(QString(), QStringLiteral("B_LT"));
    levelCombo->addItem(QString(), QStringLiteral("B_LTA"));
    // The default level is the agent's; applyConfig() sets it. Until then the
    // page offers the baseline one.
    levelCombo->setCurrentIndex(0);

    auto* levelRow = new QHBoxLayout;
    levelRow->addWidget(levelLabel);
    levelRow->addWidget(levelCombo, 1);
    layout->addLayout(levelRow);

    // TSA URL (visible only for B-T and above)
    tsaRow = new QWidget(this);
    auto* tsaLayout = new QVBoxLayout(tsaRow);
    tsaLayout->setContentsMargins(0, 0, 0, 0);
    tsaLayout->setSpacing(2);

    tsaRow->setObjectName(QStringLiteral("tsaRow"));

    auto* tsaInputRow = new QHBoxLayout;
    tsaLabel = new QLabel(tsaRow);
    tsaCombo = new QComboBox(tsaRow);
    tsaCombo->setEditable(false);
    // The configured authorities are the agent's; applyConfig() merges them in.
    tsaCombo->addItems(signing::defaultTsaUrls());
    tsaCombo->setCurrentIndex(0);
    tsaInputRow->addWidget(tsaLabel);
    tsaInputRow->addWidget(tsaCombo, 1);
    tsaLayout->addLayout(tsaInputRow);

    tsaInfoLabel = new QLabel(tsaRow);
    tsaInfoLabel->setWordWrap(true);
    tsaLayout->addWidget(tsaInfoLabel);

    layout->addWidget(tsaRow);

    connect(levelCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateTsaRowVisibility();
        emit validityChanged(isValid());
    });

    // Initialize TSA visibility based on the level the page opens at
    updateTsaRowVisibility();
    // Nothing is persisted here: `LastTsaUrl` is a read-only Config1 key the
    // agent maintains from the runs it actually performs.
    connect(tsaCombo, &QComboBox::currentIndexChanged, this, [this]() { emit validityChanged(isValid()); });

    // Output folder
    outputLabel = new QLabel(this);
    outputFolderEdit = new QLineEdit(this);
    outputFolderEdit->setReadOnly(true);
    {
        QSettings settings(settings::kOrganization, settings::kApplication);
        outputFolderEdit->setText(settings.value(settings::kSigningDefaultOutputFolder).toString());
    }

    changeFolderBtn = new QPushButton(this);

    auto* outputRow = new QHBoxLayout;
    outputRow->addWidget(outputLabel);
    outputRow->addWidget(outputFolderEdit, 1);
    outputRow->addWidget(changeFolderBtn);
    layout->addLayout(outputRow);

    layout->addStretch();

    // Connections
    connect(dropZone, &FileDropZone::filesChanged, this, &FileSelectionPage::onFilesChanged);
    connect(changeFolderBtn, &QPushButton::clicked, this, &FileSelectionPage::chooseOutputFolder);

    // Apply translations once after construction; LanguageChange branch
    // in changeEvent re-runs retranslateUi() to refresh on language switch.
    retranslateUi();
    applyThemeColors();
}

QStringList FileSelectionPage::selectedFiles() const
{
    return files;
}

QString FileSelectionPage::signatureLevel() const
{
    return levelCombo->currentData().toString();
}

QString FileSelectionPage::tsaUrl() const
{
    return tsaCombo->currentText().trimmed();
}

QString FileSelectionPage::outputFolder() const
{
    return outputFolderEdit->text();
}

bool FileSelectionPage::hasPAdESFiles() const
{
    return hasPAdES;
}

QString FileSelectionPage::firstPAdESFile() const
{
    for (int i = 0; i < fileList->count(); ++i) {
        auto* item = fileList->item(i);
        auto format =
            static_cast<LibreSCRS::AgentClient::SignatureFormat>(item->data(FileListDelegate::FormatRole).toInt());
        if (format == LibreSCRS::AgentClient::SignatureFormat::PAdES)
            return files.at(i);
    }
    return {};
}

void FileSelectionPage::setCapabilities(bool tsaOverride, bool batch)
{
    tsaOverrideAvailable = tsaOverride;
    batchCapable = batch;
    updateTsaRowVisibility();
    updateSequentialNotice();
}

void FileSelectionPage::applyConfig(const QVariantMap& config)
{
    // The agent speaks the wire's level tokens; the combo carries the UI ones.
    // An unknown or absent token leaves the page at the baseline level.
    static const QHash<QString, QString> kUiLevelToken = {
        {QStringLiteral("b-b"), QStringLiteral("B_B")},
        {QStringLiteral("b-t"), QStringLiteral("B_T")},
        {QStringLiteral("b-lt"), QStringLiteral("B_LT")},
        {QStringLiteral("b-lta"), QStringLiteral("B_LTA")},
    };
    const int levelIdx =
        levelCombo->findData(kUiLevelToken.value(config.value(QStringLiteral("DefaultLevel")).toString()));
    levelCombo->setCurrentIndex(levelIdx >= 0 ? levelIdx : 0);

    // The built-in authorities plus whatever the agent has been configured
    // with; the preselection is the one the agent last used.
    const QString currentUrl = tsaCombo->currentText();
    tsaCombo->clear();
    tsaCombo->addItems(signing::defaultTsaUrls());
    for (const QString& rawUrl : config.value(QStringLiteral("TsaUrls")).toStringList()) {
        const QString url = rawUrl.trimmed();
        if (!url.isEmpty() && tsaCombo->findText(url) < 0)
            tsaCombo->addItem(url);
    }
    const QString lastUrl = config.value(QStringLiteral("LastTsaUrl")).toString().trimmed();
    const QString preferred = lastUrl.isEmpty() ? currentUrl : lastUrl;
    const int tsaIdx = preferred.isEmpty() ? 0 : tsaCombo->findText(preferred);
    tsaCombo->setCurrentIndex(tsaIdx < 0 ? 0 : tsaIdx);

    updateTsaRowVisibility();
    emit validityChanged(isValid());
}

bool FileSelectionPage::isValid() const
{
    if (files.isEmpty())
        return false;
    // With no per-request authority to give, the page cannot be blocked by the
    // authority it is not showing — the agent's configured one serves the
    // timestamped levels.
    bool needsTsa = tsaOverrideAvailable && levelCombo->currentData().toString() != QStringLiteral("B_B");
    if (!needsTsa)
        return true;
    // See signing::isValidTsaUrl — rejects empty / non-https / missing
    // host / malformed URLs so TSA token exchange always runs over a
    // transport with integrity guarantees.
    return signing::isValidTsaUrl(tsaCombo->currentText().trimmed());
}

void FileSelectionPage::recalcHasPAdES()
{
    hasPAdES = false;
    for (int i = 0; i < fileList->count(); ++i) {
        auto format = static_cast<LibreSCRS::AgentClient::SignatureFormat>(
            fileList->item(i)->data(FileListDelegate::FormatRole).toInt());
        if (format == LibreSCRS::AgentClient::SignatureFormat::PAdES) {
            hasPAdES = true;
            return;
        }
    }
}

LibreSCRS::AgentClient::SignatureFormat FileSelectionPage::formatForFile(int index) const
{
    if (index < 0 || index >= fileList->count())
        return LibreSCRS::AgentClient::SignatureFormat::ASiCe;
    return static_cast<LibreSCRS::AgentClient::SignatureFormat>(
        fileList->item(index)->data(FileListDelegate::FormatRole).toInt());
}

LibreSCRS::AgentClient::Packaging FileSelectionPage::packagingForFile(int index) const
{
    if (index < 0 || index >= fileList->count())
        return LibreSCRS::AgentClient::Packaging::Enveloped;
    return static_cast<LibreSCRS::AgentClient::Packaging>(
        fileList->item(index)->data(FileListDelegate::PackagingRole).toInt());
}

void FileSelectionPage::onFilesChanged(const QStringList& paths)
{
    // One signing run is one ceremony, so the selection is capped at the
    // client's batch bound (see the header note: this is LC policy, and it
    // keeps every run inside the wire bound by construction). Files past the
    // cap are refused rather than silently queued for a second run, and the
    // drop zone is told so its list and this one stay the same list.
    const int bound = static_cast<int>(LibreSCRS::AgentClient::kMaxBatchDocuments);
    overBatchBound = paths.count() > bound;
    if (overBatchBound) {
        for (const QString& refused : paths.mid(bound))
            dropZone->removeFile(refused);
        files = paths.mid(0, bound);
    } else {
        files = paths;
    }

    rebuildFileList();

    // Set output folder to directory of first file, unless a default is configured
    if (!files.isEmpty() && outputFolderEdit->text().isEmpty()) {
        QFileInfo fi(files.first());
        outputFolderEdit->setText(fi.absolutePath());
    }
}

void FileSelectionPage::removeSelectedFile()
{
    int row = fileList->currentRow();
    if (row < 0 || row >= files.count())
        return;

    const QString removed = files.at(row);
    files.removeAt(row);
    dropZone->removeFile(removed);
    // The selection just shrank, so whatever was refused for exceeding the
    // cap is no longer the reason this list looks the way it does.
    overBatchBound = false;
    rebuildFileList();
}

void FileSelectionPage::rebuildFileList()
{
    dismissFormatCombos();
    fileList->clear();

    for (const QString& path : files) {
        QFileInfo fi(path);
        const QString suffix = fi.suffix();
        auto format = defaultFormatForExtension(suffix);
        // JAdES and CAdES default to DETACHED; PAdES, XAdES, ASiC-E default to ENVELOPED
        auto packaging = (format == LibreSCRS::AgentClient::SignatureFormat::JAdES ||
                          format == LibreSCRS::AgentClient::SignatureFormat::CAdES)
                             ? LibreSCRS::AgentClient::Packaging::Detached
                             : LibreSCRS::AgentClient::Packaging::Enveloped;

        auto* item = new QListWidgetItem(fileList);
        item->setData(FileListDelegate::FileNameRole, fi.fileName());
        item->setData(FileListDelegate::FileSizeRole, formatFileSize(fi.size()));
        item->setData(FileListDelegate::FileTypeRole, fi.suffix().toUpper().left(3));
        item->setData(FileListDelegate::FormatRole, static_cast<int>(format));
        item->setData(FileListDelegate::PackagingRole, static_cast<int>(packaging));
        item->setData(FileListDelegate::FormatDisplayRole, formatDisplayName(format, packaging));
    }

    recalcHasPAdES();
    updateFormatDescription();
    updateLimitsLabel();
    updateSequentialNotice();

    emit validityChanged(isValid());
}

void FileSelectionPage::updateFormatDescription()
{
    if (files.isEmpty()) {
        formatDesc->setVisible(false);
        return;
    }

    QSet<QString> formats;
    for (int i = 0; i < fileList->count(); ++i) {
        auto* item = fileList->item(i);
        formats.insert(item->data(FileListDelegate::FormatDisplayRole).toString());
    }

    QStringList formatList(formats.begin(), formats.end());
    formatList.sort();
    //% "Formats: %1"
    formatDesc->setText(qtTrId("lc-sign-formats-desc").arg(formatList.join(QStringLiteral(", "))));
    formatDesc->setVisible(true);
}

void FileSelectionPage::updateLimitsLabel()
{
    if (overBatchBound) {
        //% "One signing run takes at most %1 files. The files beyond that were not added."
        limitsLabel->setText(qtTrId("lc-sign-too-many-files").arg(LibreSCRS::AgentClient::kMaxBatchDocuments));
        return;
    }
    limitsLabel->setText(qtTrId("lc-sign-limits-info"));
}

void FileSelectionPage::updateSequentialNotice()
{
    //% "Each file is confirmed separately — %n confirmations."
    sequentialNotice->setText(qtTrId("lc-sign-sequential-notice", files.count()));
    sequentialNotice->setVisible(!batchCapable && files.count() > 1);
}

void FileSelectionPage::updateTsaRowVisibility()
{
    tsaRow->setVisible(tsaOverrideAvailable && levelCombo->currentData().toString() != QStringLiteral("B_B"));
}

void FileSelectionPage::dismissFormatCombos()
{
    if (activeFormatCombo) {
        activeFormatCombo->deleteLater();
        activeFormatCombo = nullptr;
    }
}

void FileSelectionPage::showFormatComboForItem(int row)
{
    if (row < 0 || row >= files.count())
        return;

    dismissFormatCombos();

    QFileInfo fi(files.at(row));
    auto available = availableFormatsForExtension(fi.suffix());
    if (available.size() <= 1)
        return;

    auto* item = fileList->item(row);
    auto currentFormat =
        static_cast<LibreSCRS::AgentClient::SignatureFormat>(item->data(FileListDelegate::FormatRole).toInt());
    auto currentPackaging =
        static_cast<LibreSCRS::AgentClient::Packaging>(item->data(FileListDelegate::PackagingRole).toInt());

    // Position combo at the format badge area (right side of the row)
    QRect itemRect = fileList->visualItemRect(item);
    const int comboWidth = 150;
    QPoint comboPos = fileList->viewport()->mapToGlobal(QPoint(itemRect.right() - comboWidth, itemRect.bottom()));

    auto* combo = new QComboBox(nullptr); // top-level popup
    combo->setWindowFlags(Qt::Popup);
    combo->setMinimumWidth(comboWidth);
    int currentIdx = 0;
    for (int i = 0; i < available.size(); ++i) {
        const auto& ffi = available.at(i);
        QString display = formatDisplayName(ffi.format, ffi.packaging);
        combo->addItem(display);
        combo->setItemData(i, static_cast<int>(ffi.format), Qt::UserRole);
        combo->setItemData(i, static_cast<int>(ffi.packaging), Qt::UserRole + 1);
        if (ffi.format == currentFormat && ffi.packaging == currentPackaging)
            currentIdx = i;
    }
    combo->setCurrentIndex(currentIdx);
    activeFormatCombo = combo;
    connect(combo, &QObject::destroyed, this, [this]() { activeFormatCombo = nullptr; });

    connect(combo, &QComboBox::activated, this, [this, row, combo](int idx) {
        if (row < fileList->count()) {
            auto* targetItem = fileList->item(row);
            auto format =
                static_cast<LibreSCRS::AgentClient::SignatureFormat>(combo->itemData(idx, Qt::UserRole).toInt());
            auto packaging =
                static_cast<LibreSCRS::AgentClient::Packaging>(combo->itemData(idx, Qt::UserRole + 1).toInt());
            targetItem->setData(FileListDelegate::FormatRole, static_cast<int>(format));
            targetItem->setData(FileListDelegate::PackagingRole, static_cast<int>(packaging));
            targetItem->setData(FileListDelegate::FormatDisplayRole, formatDisplayName(format, packaging));
            recalcHasPAdES();
            updateFormatDescription();
        }
        QTimer::singleShot(0, this, [this]() { dismissFormatCombos(); });
    });

    combo->move(comboPos);
    combo->show();
    combo->showPopup();
}

void FileSelectionPage::chooseOutputFolder()
{
    //% "Select output folder"
    const QString title =
        qtTrId("lc-sign-select-output-folder"); // i18n-audit: ignore D2, transient file dialog — qtTrId evaluated at
                                                // click time, dialog discarded after exec()
    const QString dir = QFileDialog::getExistingDirectory(this, title, outputFolderEdit->text());
    if (!dir.isEmpty()) {
        outputFolderEdit->setText(dir);
        QSettings settings(settings::kOrganization, settings::kApplication);
        settings.setValue(settings::kSigningDefaultOutputFolder, dir);
    }
}

QString FileSelectionPage::formatFileSize(qint64 bytes) const
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

void FileSelectionPage::applyThemeColors()
{
    auto placeholderColor = palette().color(QPalette::PlaceholderText).name();
    formatDesc->setStyleSheet(QString("color: %1; font-style: italic;").arg(placeholderColor));
    tsaInfoLabel->setStyleSheet(QString("color: %1; font-style: italic; font-size: 10px;").arg(placeholderColor));
    fileList->setStyleSheet(QString("QListWidget { border: 1px solid %1; border-radius: 6px; }")
                                .arg(palette().color(QPalette::Mid).name()));
}

void FileSelectionPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        applyThemeColors();
    else if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void FileSelectionPage::retranslateUi()
{
    fileList->setToolTip(qtTrId("lc-sign-filelist-tooltip"));
    updateFormatDescription();
    levelLabel->setText(qtTrId("lc-sign-level-label"));
    tsaLabel->setText(qtTrId("lc-sign-tsa-label"));
    tsaInfoLabel->setText(qtTrId("lc-sign-tsa-info"));
    outputLabel->setText(qtTrId("lc-sign-output-folder"));
    changeFolderBtn->setText(qtTrId("lc-sign-change-folder"));

    int idx = levelCombo->currentIndex();
    levelCombo->setItemText(0, qtTrId("lc-sign-level-bb"));
    levelCombo->setItemText(1, qtTrId("lc-sign-level-bt"));
    levelCombo->setItemText(2, qtTrId("lc-sign-level-blt"));
    levelCombo->setItemText(3, qtTrId("lc-sign-level-blta"));
    levelCombo->setCurrentIndex(idx);
    updateLimitsLabel();
    updateSequentialNotice();
}
