// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CHANGEPINDLG_H
#define CHANGEPINDLG_H

#include <QDialog>
#include <memory>
#include <string>

class QAction;
class QLabel;
class QLineEdit;
class QDialogButtonBox;
class QPushButton;
class EIdReader;

class ChangePinDlg : public QDialog
{
    Q_OBJECT
public:
    explicit ChangePinDlg(const std::string& readerName, QWidget *parent = nullptr);
    ~ChangePinDlg();

private slots:
    void onOkClicked();
    void onPinTriesLeftRead(int triesLeft, bool blocked);
    void onPinChangeSuccess();
    void onPinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage);
    void validateForm();

private:
    void addToggleVisibilityAction(QLineEdit *edit);
    bool isValidPinLength(const QString &pin) const;
    QLabel *retriesLabel;
    QLineEdit *currentPinEdit;
    QLineEdit *newPinEdit;
    QLineEdit *confirmPinEdit;
    QLabel *statusLabel;
    QDialogButtonBox *buttonBox;
    QPushButton *okButton;

    std::unique_ptr<EIdReader> eidReader;
};

#endif // CHANGEPINDLG_H
