// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QEvent>
#include <QMap>
#include <QWidget>

class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;

class EMRTDAuthWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EMRTDAuthWidget(QWidget* parent = nullptr);
    void setDefaultTab(bool paceSupported);

signals:
    void credentialsEntered(const QMap<QString, QString>& credentials);

protected:
    void changeEvent(QEvent* event) override;

public slots:
    void onAuthFailed(const QString& errorMessage);

private slots:
    void onAuthenticateClicked();
    void validateForm();

private:
    QTabWidget* tabWidget;
    QLineEdit* canEdit;
    QLineEdit* docNumberEdit;
    QDateEdit* dobEdit;
    QDateEdit* expiryEdit;
    QPushButton* authButton;
    QLabel* statusLabel;
};
