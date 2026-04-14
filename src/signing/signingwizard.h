// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <libresign/types.h>
#include <plugin/card_plugin.h>

#include <QDialog>
#include <QFuture>

namespace libresign {
class SigningService;
}

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
    SigningWizard(const plugin::CertificateData& cert, const std::string& readerName,
                  libresign::SigningService* signingService, QWidget* parent = nullptr);
    ~SigningWizard() override;

    void setTrustConfig(const libresign::TrustConfig& config);
    bool waitForPrefetch();

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
    plugin::CertificateData certificate;
    std::string readerName;
    libresign::SigningService* signingService;
    QFuture<bool> prefetchFuture;
    bool placementShown = false;
};
