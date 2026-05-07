// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Secure/String.h>

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
    /// @brief Emitted when the user has entered both PINs and confirmed.
    ///
    /// The PIN material is carried as @ref LibreSCRS::Secure::String so that
    /// the cleansing-on-destruction guarantee survives the Qt signal/slot
    /// boundary. QString-based PIN flow leaks plaintext bytes into Qt's
    /// implicitly-shared storage and the event queue; the Secure::String
    /// flow keeps every copy under @c secure_allocator. Receivers MUST
    /// register the metatype once at startup via
    /// @c qRegisterMetaType<LibreSCRS::Secure::String>().
    void pinChangeRequested(const LibreSCRS::Secure::String& oldPin, const LibreSCRS::Secure::String& newPin);

public slots:
    void onPinRetriesLeftRead(int retriesLeft, bool blocked);
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
