// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Plugin/PluginTypes.h>
#include <LibreSCRS/Signing/Enums.h>
#include <LibreSCRS/Signing/VisualSignatureParams.h>
#include <LibreSCRS/SmartCard/CardSession.h>

#include <QWidget>

#include <atomic>
#include <memory>
#include <optional>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QThread;

namespace LibreSCRS::Signing {
class SigningService;
}
namespace LibreSCRS::Plugin {
class CardPlugin;
}

struct FileSignInfo
{
    QString filePath;
    LibreSCRS::Signing::SignatureFormat format;
    LibreSCRS::Signing::PackagingMode packaging;
};

class SignPage : public QWidget
{
    Q_OBJECT
public:
    explicit SignPage(QWidget* parent = nullptr);
    ~SignPage() override;

    // All inputs the sign page needs to configure itself for one signing run.
    // Bundling these into a struct lets callers (the wizard) build the
    // configuration in steps without juggling a many-arg call, and lets us
    // grow the inputs later without breaking call sites.
    struct Config
    {
        // Owned by value: a previous version held `const CertificateData&`,
        // which made Config a dangling-reference trap if the wizard's
        // certificate source went out of scope before configure() was called.
        // Callers now pay the cost of one CertificateData copy per signing run.
        LibreSCRS::Plugin::CertificateData certificate;
        std::string readerName;
        QList<FileSignInfo> fileInfos;
        QString level;
        QString outputFolder;
        // std::optional: absent signals "no visual signature" and the request
        // omits it entirely (invisible-signature path). VisualSignatureParams
        // is pimpl-backed value-type; the optional holds it directly.
        std::optional<LibreSCRS::Signing::VisualSignatureParams> visual;
        std::string tsaUrl;
        // Shared ownership: both the CardPlugin and the active CardSession
        // are held by shared_ptr so the signing worker lambda can extend
        // their lifetime past a wizard close.
        std::shared_ptr<LibreSCRS::Plugin::CardPlugin> cardPlugin;
        std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
    };

    void configure(Config cfg);
    void setSigningService(std::shared_ptr<LibreSCRS::Signing::SigningService> svc);
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
    static LibreSCRS::Signing::SignatureLevel parseLevel(const QString& level);
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

    std::shared_ptr<LibreSCRS::Signing::SigningService> signingService;
    LibreSCRS::Plugin::CertificateData certificate;
    std::string readerName;
    QList<FileSignInfo> fileInfos;
    QString sigLevel;
    QString outputDir;
    std::optional<LibreSCRS::Signing::VisualSignatureParams> visualParams;
    std::string tsaUrl;
    // Shared ownership: the signing worker lambda copies these shared_ptrs
    // so the CardPlugin and CardSession outlive this widget if the user
    // closes the wizard before the worker finishes.
    std::shared_ptr<LibreSCRS::Plugin::CardPlugin> cardPlugin;
    std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
    QThread* workerThread = nullptr;
    std::atomic<bool> signingInProgress{false};
    std::atomic<bool> signingComplete{false};
    std::atomic<int> failedCount{0};
};
