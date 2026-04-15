// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "fileselectionpage.h"

#include "certutils.h"
#include "defaults.h"
#include "filedropzone.h"
#include "filelistdelegate.h"
#include "settings/settingskeys.h"

#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
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

libresign::SignatureFormat FileSelectionPage::defaultFormatForExtension(const QString& suffix)
{
    const QString s = suffix.toLower();
    if (s == QStringLiteral("pdf"))
        return libresign::SignatureFormat::PAdES;
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return libresign::SignatureFormat::XAdES;
    if (s == QStringLiteral("json"))
        return libresign::SignatureFormat::JAdES;
    return libresign::SignatureFormat::ASiC_E;
}

QList<FileFormatInfo> FileSelectionPage::availableFormatsForExtension(const QString& suffix)
{
    using F = libresign::SignatureFormat;
    using P = libresign::SignaturePackaging;
    const QString s = suffix.toLower();

    if (s == QStringLiteral("pdf"))
        return {{F::PAdES, P::ENVELOPED}, {F::ASiC_E, P::ENVELOPED}};
    if (s == QStringLiteral("xml") || s == QStringLiteral("xsd") || s == QStringLiteral("xsl"))
        return {
            {F::XAdES, P::ENVELOPED},
            {F::XAdES, P::DETACHED},
            {F::ASiC_E, P::ENVELOPED},
            {F::CAdES, P::DETACHED},
        };
    if (s == QStringLiteral("json"))
        return {
            {F::JAdES, P::DETACHED},
            {F::ASiC_E, P::ENVELOPED},
            {F::CAdES, P::DETACHED},
        };
    return {{F::ASiC_E, P::ENVELOPED}, {F::CAdES, P::DETACHED}};
}

QString FileSelectionPage::formatDisplayName(libresign::SignatureFormat format, libresign::SignaturePackaging packaging)
{
    using F = libresign::SignatureFormat;
    using P = libresign::SignaturePackaging;
    switch (format) {
    case F::PAdES:
        return QStringLiteral("PAdES");
    case F::CAdES:
        return QStringLiteral("CAdES (.p7s)");
    case F::XAdES:
        return packaging == P::ENVELOPED ? QStringLiteral("XAdES (enveloped)") : QStringLiteral("XAdES (detached)");
    case F::ASiC_E:
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

    // File size limits info
    limitsLabel = new QLabel(qtTrId("lc-sign-limits-info"), this);
    limitsLabel->setWordWrap(true);
    auto limitsFont = limitsLabel->font();
    limitsFont.setPointSize(limitsFont.pointSize() - 1);
    limitsLabel->setFont(limitsFont);
    limitsLabel->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(limitsLabel);

    // File list (Delete key removes selected file)
    fileList = new QListWidget(this);
    fileList->setMinimumHeight(80);
    fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    fileList->setItemDelegate(new FileListDelegate(fileList));
    //% "Select a file and press Delete to remove it"
    fileList->setToolTip(qtTrId("lc-sign-filelist-tooltip"));
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

    // Signature level
    //% "Signature level:"
    levelLabel = new QLabel(qtTrId("lc-sign-level-label"), this);
    levelCombo = new QComboBox(this);
    //% "B-B (Basic)"
    levelCombo->addItem(qtTrId("lc-sign-level-bb"), QStringLiteral("B_B"));
    //% "B-T (with Timestamp)"
    levelCombo->addItem(qtTrId("lc-sign-level-bt"), QStringLiteral("B_T"));
    //% "B-LT (Long Term)"
    levelCombo->addItem(qtTrId("lc-sign-level-blt"), QStringLiteral("B_LT"));
    //% "B-LTA (Long Term with Archive)"
    levelCombo->addItem(qtTrId("lc-sign-level-blta"), QStringLiteral("B_LTA"));
    {
        QSettings settings(settings::kOrganization, settings::kApplication);
        QString defaultLevel = settings.value(settings::kSigningDefaultLevel, QStringLiteral("B_B")).toString();
        int idx = levelCombo->findData(defaultLevel);
        levelCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    auto* levelRow = new QHBoxLayout;
    levelRow->addWidget(levelLabel);
    levelRow->addWidget(levelCombo, 1);
    layout->addLayout(levelRow);

    // TSA URL (visible only for B-T and above)
    tsaRow = new QWidget(this);
    auto* tsaLayout = new QVBoxLayout(tsaRow);
    tsaLayout->setContentsMargins(0, 0, 0, 0);
    tsaLayout->setSpacing(2);

    auto* tsaInputRow = new QHBoxLayout;
    //% "TSA server:"
    tsaLabel = new QLabel(qtTrId("lc-sign-tsa-label"), tsaRow);
    tsaCombo = new QComboBox(tsaRow);
    tsaCombo->setEditable(false);
    {
        QSettings settings(settings::kOrganization, settings::kApplication);
        QStringList saved = settings.value(settings::kSigningTsaUrls).toStringList();
        QString lastUrl = settings.value(settings::kSigningTsaLastUrl).toString();

        tsaCombo->addItems(signing::defaultTsaUrls());
        for (const QString& url : saved) {
            if (!url.trimmed().isEmpty() && tsaCombo->findText(url) < 0)
                tsaCombo->addItem(url);
        }

        int idx = lastUrl.isEmpty() ? 0 : tsaCombo->findText(lastUrl);
        tsaCombo->setCurrentIndex(idx < 0 ? 0 : idx);
    }
    tsaInputRow->addWidget(tsaLabel);
    tsaInputRow->addWidget(tsaCombo, 1);
    tsaLayout->addLayout(tsaInputRow);

    //% "For a qualified signature, use a qualified TSA from your certificate authority"
    tsaInfoLabel = new QLabel(qtTrId("lc-sign-tsa-info"), tsaRow);
    tsaInfoLabel->setWordWrap(true);
    tsaLayout->addWidget(tsaInfoLabel);

    layout->addWidget(tsaRow);

    connect(levelCombo, &QComboBox::currentIndexChanged, this, [this]() {
        tsaRow->setVisible(levelCombo->currentData().toString() != QStringLiteral("B_B"));
        emit validityChanged(isValid());
    });

    // Initialize TSA visibility based on loaded default level
    tsaRow->setVisible(levelCombo->currentData().toString() != QStringLiteral("B_B"));
    connect(tsaCombo, &QComboBox::currentIndexChanged, this, [this]() {
        emit validityChanged(isValid());
        // Persist last selected TSA
        QSettings settings(settings::kOrganization, settings::kApplication);
        settings.setValue(settings::kSigningTsaLastUrl, tsaCombo->currentText().trimmed());
    });

    // Output folder
    //% "Output folder:"
    outputLabel = new QLabel(qtTrId("lc-sign-output-folder"), this);
    outputFolderEdit = new QLineEdit(this);
    outputFolderEdit->setReadOnly(true);
    {
        QSettings settings(settings::kOrganization, settings::kApplication);
        outputFolderEdit->setText(settings.value(settings::kSigningDefaultOutputFolder).toString());
    }

    //% "Change..."
    changeFolderBtn = new QPushButton(qtTrId("lc-sign-change-folder"), this);

    auto* outputRow = new QHBoxLayout;
    outputRow->addWidget(outputLabel);
    outputRow->addWidget(outputFolderEdit, 1);
    outputRow->addWidget(changeFolderBtn);
    layout->addLayout(outputRow);

    layout->addStretch();

    // Connections
    connect(dropZone, &FileDropZone::filesChanged, this, &FileSelectionPage::onFilesChanged);
    connect(changeFolderBtn, &QPushButton::clicked, this, &FileSelectionPage::chooseOutputFolder);

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
        auto format = static_cast<libresign::SignatureFormat>(item->data(FileListDelegate::FormatRole).toInt());
        if (format == libresign::SignatureFormat::PAdES)
            return files.at(i);
    }
    return {};
}

bool FileSelectionPage::isValid() const
{
    if (files.isEmpty())
        return false;
    bool needsTsa = levelCombo->currentData().toString() != QStringLiteral("B_B");
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
        auto format =
            static_cast<libresign::SignatureFormat>(fileList->item(i)->data(FileListDelegate::FormatRole).toInt());
        if (format == libresign::SignatureFormat::PAdES) {
            hasPAdES = true;
            return;
        }
    }
}

libresign::SignatureFormat FileSelectionPage::formatForFile(int index) const
{
    if (index < 0 || index >= fileList->count())
        return libresign::SignatureFormat::ASiC_E;
    return static_cast<libresign::SignatureFormat>(fileList->item(index)->data(FileListDelegate::FormatRole).toInt());
}

libresign::SignaturePackaging FileSelectionPage::packagingForFile(int index) const
{
    if (index < 0 || index >= fileList->count())
        return libresign::SignaturePackaging::ENVELOPED;
    return static_cast<libresign::SignaturePackaging>(
        fileList->item(index)->data(FileListDelegate::PackagingRole).toInt());
}

void FileSelectionPage::onFilesChanged(const QStringList& paths)
{
    files = paths;
    rebuildFileList();

    // Set output folder to directory of first file, unless a default is configured
    if (!paths.isEmpty() && outputFolderEdit->text().isEmpty()) {
        QFileInfo fi(paths.first());
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
        auto packaging = (format == libresign::SignatureFormat::JAdES || format == libresign::SignatureFormat::CAdES)
                             ? libresign::SignaturePackaging::DETACHED
                             : libresign::SignaturePackaging::ENVELOPED;

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
    auto currentFormat = static_cast<libresign::SignatureFormat>(item->data(FileListDelegate::FormatRole).toInt());
    auto currentPackaging =
        static_cast<libresign::SignaturePackaging>(item->data(FileListDelegate::PackagingRole).toInt());

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
            auto format = static_cast<libresign::SignatureFormat>(combo->itemData(idx, Qt::UserRole).toInt());
            auto packaging = static_cast<libresign::SignaturePackaging>(combo->itemData(idx, Qt::UserRole + 1).toInt());
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
    const QString dir =
        QFileDialog::getExistingDirectory(this, qtTrId("lc-sign-select-output-folder"), outputFolderEdit->text());
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
    else if (event->type() == QEvent::LanguageChange) {
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
        limitsLabel->setText(qtTrId("lc-sign-limits-info"));
    }
    QWidget::changeEvent(event);
}
