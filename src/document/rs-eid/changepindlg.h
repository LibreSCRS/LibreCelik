// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialog>

class QAction;
class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QPushButton;

class ChangePinDlg : public QDialog
{
    Q_OBJECT
public:
    explicit ChangePinDlg(const QString& pinLabel, bool isTransport, int minLen = 4, int maxLen = 8,
                          QWidget* parent = nullptr);
    ~ChangePinDlg();

signals:
    void pinChangeRequested(const QString& oldPin, const QString& newPin);

public slots:
    void onPinTriesLeftRead(int triesLeft, bool blocked);
    void onPinChangeSuccess();
    void onPinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage);

private slots:
    void onOkClicked();
    void validateForm();

private:
    bool isValidPinLength(const QString& pin) const;
    QLabel* retriesLabel;
    QLineEdit* currentPinEdit;
    QLineEdit* newPinEdit;
    QLineEdit* confirmPinEdit;
    QLabel* statusLabel;
    QDialogButtonBox* buttonBox;
    QPushButton* okButton;
    int pinMinLength;
    int pinMaxLength;
};
