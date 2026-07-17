// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "emrtdcredentials.h"

#include <QEvent>
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
    /// @brief Emitted on auth-button click with the user-entered CAN/MRZ
    ///        wrapped in cleansing @ref LibreSCRS::Secure::String storage.
    /// @note  Connected via @c Qt::QueuedConnection in @c librecelik.cpp;
    ///        the carrier travels by-value across the queue hop, with
    ///        Secure::String's copy ctor allocating a fresh cleansed block
    ///        per copy. The original QLineEdits are cleared immediately
    ///        after the secure copy is built — read-then-hide ordering.
    void credentialsEntered(const EmrtdCredentials& credentials);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

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
