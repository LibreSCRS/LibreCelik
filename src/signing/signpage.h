// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <libresign/types.h>
#include <plugin/card_plugin.h>

#include <QWidget>

#include <atomic>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QThread;

namespace libresign {
class SigningService;
}

struct FileSignInfo
{
    QString filePath;
    libresign::SignatureFormat format;
    libresign::SignaturePackaging packaging;
};

class SignPage : public QWidget
{
    Q_OBJECT
public:
    explicit SignPage(QWidget* parent = nullptr);
    ~SignPage() override;

    // All inputs the sign page needs to configure itself for one signing run.
    // Bundling these into a struct lets callers (the wizard) build the
    // configuration in steps without juggling a 7-arg call, and lets us
    // grow the inputs later without breaking call sites.
    struct Config
    {
        // Owned by value: a previous version held `const CertificateData&`,
        // which made Config a dangling-reference trap if the wizard's
        // certificate source went out of scope before configure() was called.
        // Callers now pay the cost of one CertificateData copy per signing run.
        plugin::CertificateData certificate;
        std::string readerName;
        QList<FileSignInfo> fileInfos;
        QString level;
        QString outputFolder;
        libresign::VisualSignatureParams visual;
        std::string tsaUrl;
    };

    void configure(const Config& cfg);
    void setSigningService(libresign::SigningService* svc);
    void setTrustConfig(const libresign::TrustConfig& config);
    void setPrefetchCallback(std::function<bool()> cb);
    bool hasPinInput() const;
    void focusPin();
    bool isSigningComplete() const;
    bool isSigningInProgress() const;
    bool hasFailures() const;

signals:
    void pinReady(bool ready);
    void signingStarted();
    void signingFinished(int succeeded, int failed);

public slots:
    void startSigning();

protected:
    void changeEvent(QEvent* event) override;

private:
    static libresign::SignatureLevel parseLevel(const QString& level);
    QString buildOutputPath(const FileSignInfo& info) const;
    void clearPin();
    void applyThemeColors();
    void addResultItem(const QString& icon, const QString& colorHex, const QString& message);

    QWidget* summaryCard = nullptr;
    QLabel* certHeaderLabel = nullptr;
    QLabel* certValueLabel = nullptr;
    QLabel* filesHeaderLabel = nullptr;
    QLabel* filesValueLabel = nullptr;
    QLabel* levelHeaderLabel = nullptr;
    QLabel* levelValueLabel = nullptr;

    QWidget* pinRow = nullptr;
    QLabel* pinLabel = nullptr;
    QLineEdit* pinEdit = nullptr;

    QWidget* canRow = nullptr;
    QLabel* canLabel = nullptr;
    QLineEdit* canEdit = nullptr;

    QWidget* progressWidget = nullptr;
    QLabel* progressLabel = nullptr;
    QProgressBar* progressBar = nullptr;

    QListWidget* resultsList = nullptr;

    libresign::SigningService* signingService = nullptr;
    plugin::CertificateData certificate;
    std::string readerName;
    QList<FileSignInfo> fileInfos;
    QString sigLevel;
    QString outputDir;
    libresign::VisualSignatureParams visualParams;
    std::string tsaUrl;
    libresign::TrustConfig trustConfig;
    std::function<bool()> prefetchCallback;
    QThread* workerThread = nullptr;
    std::atomic<bool> signingInProgress{false};
    std::atomic<bool> signingComplete{false};
    std::atomic<int> failedCount{0};
};
