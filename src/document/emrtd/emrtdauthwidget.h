// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QMap>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class EMRTDAuthWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EMRTDAuthWidget(QWidget* parent = nullptr);

signals:
    void credentialsEntered(const QMap<QString, QString>& credentials);

public slots:
    void onAuthFailed(const QString& errorMessage);

private slots:
    void onAuthenticateClicked();
    void validateForm();

private:
    QLineEdit* canEdit;
    QPushButton* authButton;
    QLabel* statusLabel;
};
