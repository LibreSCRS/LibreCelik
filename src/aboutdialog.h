// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTextBrowser;

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void loadLicense(int index);

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
    QLabel* licenseMiddlewareLabel = nullptr;
    QLabel* licenseOpenScLabel = nullptr;
    QLabel* licenseOpenSslLabel = nullptr;
    QLabel* licenseJsonLabel = nullptr;
    QLabel* licenseMinizLabel = nullptr;
    QLabel* licenseZlibLabel = nullptr;
    QLabel* licenseLiberationSansLabel = nullptr;
    QLabel* sourceOfferLabel = nullptr;
    QComboBox* licenseCombo = nullptr;
    QTextBrowser* licenseBrowser = nullptr;
};
