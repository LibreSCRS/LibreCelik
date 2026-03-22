// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CHANGEPINDLG_H
#define CHANGEPINDLG_H

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
    explicit ChangePinDlg(const QString& pinLabel, bool isTransport, QWidget* parent = nullptr);
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
    void addToggleVisibilityAction(QLineEdit* edit);
    bool isValidPinLength(const QString& pin) const;
    QLabel* retriesLabel;
    QLineEdit* currentPinEdit;
    QLineEdit* newPinEdit;
    QLineEdit* confirmPinEdit;
    QLabel* statusLabel;
    QDialogButtonBox* buttonBox;
    QPushButton* okButton;
};

#endif // CHANGEPINDLG_H
