// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <QEvent>
#include <QWidget>
#include <LibreSCRS/Plugin/SecurityCheck.h>

class QLabel;
class QVBoxLayout;
class CollapsibleSection;

class SecurityStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SecurityStatusWidget(QWidget* parent = nullptr);
    void setSecurityStatus(const LibreSCRS::Plugin::SecurityStatus& status);

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildLayout();
    void retranslateUi();
    QWidget* createStatusRow(const QString& label, LibreSCRS::Plugin::SecurityCheck::Status status);
    QString statusColor(LibreSCRS::Plugin::SecurityCheck::Status status) const;
    QString statusText(LibreSCRS::Plugin::SecurityCheck::Status status) const;

    QVBoxLayout* mainLayout = nullptr;
    CollapsibleSection* section = nullptr;
    QLabel* integrityIcon = nullptr;
    QLabel* integrityLabel = nullptr;
    QLabel* authenticityIcon = nullptr;
    QLabel* authenticityLabel = nullptr;
    QLabel* genuinenessIcon = nullptr;
    QLabel* genuinenessLabel = nullptr;
    QWidget* detailWidget = nullptr;
};
