// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QDialog>
#include <QMap>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QStackedWidget;

class EMRTDAuthDlg : public QDialog
{
    Q_OBJECT
public:
    explicit EMRTDAuthDlg(bool paceSupported, QWidget* parent = nullptr);

signals:
    void credentialsEntered(const QMap<QString, QString>& credentials);

public slots:
    void onAuthSuccess();
    void onAuthFailed(const QString& errorMessage);

private slots:
    void onAuthenticateClicked();
    void onAuthModeChanged();
    void validateForm();
    void updateMrzPreview();

private:
    QString dateToYYMMDD(const QString& ddmmyyyy) const;

    QRadioButton* canRadio;
    QRadioButton* mrzRadio;
    QStackedWidget* inputStack;
    QLineEdit* canEdit;
    QLineEdit* docNumberEdit;
    QLineEdit* dobEdit;
    QLineEdit* expiryEdit;
    QLabel* mrzPreview;
    QLabel* statusLabel;
    QDialogButtonBox* buttonBox;
    QPushButton* authButton;
};
