// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me
#pragma once

#include <QWidget>
#include <plugin/security_check.h>

class QLabel;
class QVBoxLayout;
class CollapsibleSection;

class SecurityStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SecurityStatusWidget(QWidget* parent = nullptr);
    void setSecurityStatus(const plugin::SecurityStatus& status);

private:
    void buildLayout();
    QWidget* createStatusRow(const QString& label, plugin::SecurityCheck::Status status);
    QString statusColor(plugin::SecurityCheck::Status status) const;
    QString statusText(plugin::SecurityCheck::Status status) const;

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
