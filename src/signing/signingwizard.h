// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Plugin/CardPlugin.h>
#include <LibreSCRS/Plugin/PluginTypes.h>
#include <LibreSCRS/SmartCard/CardSession.h>

#include <QDialog>

#include <cstdint>
#include <memory>

namespace LibreSCRS::Signing {
class SigningService;
enum class SignatureFormat : std::uint8_t;
enum class PackagingMode : std::uint8_t;
} // namespace LibreSCRS::Signing

struct FileSignInfo;
class FileSelectionPage;
class SignaturePlacementPage;
class SignPage;
class WizardHeaderWidget;
class QStackedWidget;
class QPushButton;

class SigningWizard : public QDialog
{
    Q_OBJECT
public:
    SigningWizard(const LibreSCRS::Plugin::CertificateData& cert, const std::string& readerName,
                  std::shared_ptr<LibreSCRS::Signing::SigningService> signingService,
                  std::shared_ptr<LibreSCRS::Plugin::CardPlugin> cardPlugin,
                  std::shared_ptr<LibreSCRS::SmartCard::CardSession> session, QWidget* parent = nullptr);
    ~SigningWizard() override;

    std::shared_ptr<LibreSCRS::Plugin::CardPlugin> plugin() const
    {
        return cardPlugin;
    }

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void goNext();
    void goBack();
    void updateButtons();
    QString certificateCN() const;
    QList<FileSignInfo> buildFileInfoList() const;

    WizardHeaderWidget* headerWidget = nullptr;
    QStackedWidget* stack = nullptr;
    FileSelectionPage* filePage = nullptr;
    SignaturePlacementPage* placementPage = nullptr;
    SignPage* signPage = nullptr;
    QPushButton* backBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QPushButton* cancelBtn = nullptr;
    LibreSCRS::Plugin::CertificateData certificate;
    std::string readerName;
    std::shared_ptr<LibreSCRS::Signing::SigningService> signingService;
    std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
    std::shared_ptr<LibreSCRS::Plugin::CardPlugin> cardPlugin;
    bool placementShown = false;
};
