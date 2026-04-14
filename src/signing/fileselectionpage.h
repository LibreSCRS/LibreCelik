// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <libresign/types.h>

#include <QWidget>

class QEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QWidget;
class FileDropZone;

struct FileFormatInfo
{
    libresign::SignatureFormat format;
    libresign::SignaturePackaging packaging;
};

class FileSelectionPage : public QWidget
{
    Q_OBJECT
public:
    explicit FileSelectionPage(QWidget* parent = nullptr);

    QStringList selectedFiles() const;
    QString signatureLevel() const;
    QString tsaUrl() const;
    QString outputFolder() const;
    bool hasPAdESFiles() const;
    QString firstPAdESFile() const;
    libresign::SignatureFormat formatForFile(int index) const;
    libresign::SignaturePackaging packagingForFile(int index) const;

signals:
    void validityChanged(bool valid);

protected:
    void changeEvent(QEvent* event) override;

private:
    void onFilesChanged(const QStringList& paths);
    void removeSelectedFile();
    void rebuildFileList();
    void chooseOutputFolder();
    void applyThemeColors();
    void updateFormatDescription();
    void showFormatComboForItem(int row);
    void dismissFormatCombos();
    void recalcHasPAdES();
    bool isValid() const;
    QString formatFileSize(qint64 bytes) const;

    static libresign::SignatureFormat defaultFormatForExtension(const QString& suffix);
    static QList<FileFormatInfo> availableFormatsForExtension(const QString& suffix);
    static QString formatDisplayName(libresign::SignatureFormat format, libresign::SignaturePackaging packaging);

    FileDropZone* dropZone = nullptr;
    QListWidget* fileList = nullptr;
    QLabel* formatDesc = nullptr;
    QComboBox* levelCombo = nullptr;
    QWidget* tsaRow = nullptr;
    QComboBox* tsaCombo = nullptr;
    QLabel* tsaInfoLabel = nullptr;
    QLineEdit* outputFolderEdit = nullptr;
    QPushButton* changeFolderBtn = nullptr;
    QLabel* levelLabel = nullptr;
    QLabel* tsaLabel = nullptr;
    QLabel* outputLabel = nullptr;
    QComboBox* activeFormatCombo = nullptr;
    QLabel* limitsLabel = nullptr;
    QStringList files;
    bool hasPAdES = false;
};
