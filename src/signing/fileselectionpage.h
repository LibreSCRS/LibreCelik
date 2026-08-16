// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/SignOptions.h>

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
    LibreSCRS::AgentClient::SignatureFormat format;
    LibreSCRS::AgentClient::Packaging packaging;
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
    LibreSCRS::AgentClient::SignatureFormat formatForFile(int index) const;
    LibreSCRS::AgentClient::Packaging packagingForFile(int index) const;

    // Tell the page what the connected agent can actually do.
    //
    // `tsaOverride` is the "tsa-url" feature token: without it the agent
    // refuses a per-request timestamp authority, so the row that picks one is
    // hidden. The level combo keeps offering the timestamped levels — the
    // agent's own configured authority serves those, the caller just cannot
    // override it.
    //
    // `batch` is the "batch-sign" feature token. A pre-feature agent still
    // gets a multi-file selection; the run degrades to one sign() per file,
    // and the page says so rather than silently dropping the capability.
    //
    // Until this is called the page behaves as if both are available.
    void setCapabilities(bool tsaOverride, bool batch);

    // Pure routing table, public so the format-routing suite pins the real
    // one rather than a copy of it.
    static LibreSCRS::AgentClient::SignatureFormat defaultFormatForExtension(const QString& suffix);

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
    void retranslateUi();
    void updateFormatDescription();
    void updateLimitsLabel();
    void updateSequentialNotice();
    void updateTsaRowVisibility();
    void showFormatComboForItem(int row);
    void dismissFormatCombos();
    void recalcHasPAdES();
    bool isValid() const;
    QString formatFileSize(qint64 bytes) const;

    static QList<FileFormatInfo> availableFormatsForExtension(const QString& suffix);
    static QString formatDisplayName(LibreSCRS::AgentClient::SignatureFormat format,
                                     LibreSCRS::AgentClient::Packaging packaging);

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
    QLabel* sequentialNotice = nullptr;
    QStringList files;
    bool hasPAdES = false;
    // Capping the SELECTION at kMaxBatchDocuments is an LC user-experience
    // policy, not a wire rule: the client bound governs ONE signBatch() call.
    // The policy is "one batch ceremony per homogeneous run", and it makes
    // every run this page produces fit inside the wire bound by construction,
    // whichever way the run is later partitioned.
    bool overBatchBound = false;
    bool tsaOverrideAvailable = true;
    bool batchCapable = true;
};
