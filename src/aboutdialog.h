// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTextBrowser;

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    /// @param agentVersion  Agent version as the caller knows it, empty when
    ///                      no agent answers. The dialog is modal and short
    ///                      lived, so the value is a snapshot, not a feed.
    explicit AboutDialog(const QString& agentVersion, QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void loadLicense(int index);

    QString agentVersion;

    QTabWidget* tabs = nullptr;

    // About tab
    QLabel* iconLabel = nullptr;
    QLabel* appNameLabel = nullptr;
    QLabel* versionLabel = nullptr;
    QLabel* descriptionLabel = nullptr;
    QLabel* copyrightLabel = nullptr;
    QLabel* componentsLabel = nullptr;
    QLabel* linksLabel = nullptr;
    QLabel* motivationLabel = nullptr;
    QPushButton* donateButton = nullptr;

    // Credits tab
    QLabel* authorsHeading = nullptr;
    QLabel* authorNameLabel = nullptr;
    QLabel* authorRoleLabel = nullptr;
    QLabel* authorEmailLabel = nullptr;

    // License tab
    QLabel* licenseLibreCelikLabel = nullptr;
    QLabel* licenseAgentLabel = nullptr;
    QLabel* licenseOpenSslLabel = nullptr;
    QLabel* licenseQtLabel = nullptr;
    QLabel* licenseCurlLabel = nullptr;
    QLabel* licenseLibXml2Label = nullptr;
    QLabel* sourceOfferLabel = nullptr;
    QComboBox* licenseCombo = nullptr;
    QTextBrowser* licenseBrowser = nullptr;
    int fullNoticesIndex = -1;
};
