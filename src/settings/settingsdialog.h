// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void languageChanged(const QString& locale);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void loadSettings();
    void saveSettings();
    void populateTsaList();
    void onTsaAddRequested();
    void populateTlList();
    void onTlAddRequested();

    QTabWidget* tabs = nullptr;

    // General tab
    QLabel* languageLabel = nullptr;
    QComboBox* languageCombo = nullptr;
    QString originalLocale;

    // Signing tab
    QLabel* defaultLevelLabel = nullptr;
    QLabel* defaultOutputLabel = nullptr;
    QLabel* tsaServersLabel = nullptr;
    QComboBox* defaultLevelCombo = nullptr;
    QLineEdit* defaultOutputFolder = nullptr;
    QPushButton* browseOutputBtn = nullptr;
    QListWidget* tsaList = nullptr;

    // Trust tab
    QLabel* tlServersLabel = nullptr;
    QLabel* cacheDirLabel = nullptr;
    QPushButton* browseCacheBtn = nullptr;
    QListWidget* tlList = nullptr;
    QLineEdit* cacheDir = nullptr;
};
