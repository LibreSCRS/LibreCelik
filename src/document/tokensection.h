// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef TOKENSECTION_H
#define TOKENSECTION_H

#include <string>
#include <vector>
#include <plugin/card_plugin.h>
#include "utils/collapsiblesection.h"

class QTreeWidget;
class QTreeWidgetItem;

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    explicit TokenSection(std::string certFolderPath, QWidget* parent = nullptr);

    void setPINVisible(bool visible);

public slots:
    void setCertificates(const std::vector<plugin::CertificateData>& certList);
    void setPINStatus(int triesLeft, bool blocked);
    void setPINList(const std::vector<plugin::PinStatusEntry>& pins);

signals:
    void changePINRequested(uint8_t pinReference, const QString& pinLabel, bool isTransport);

private slots:
    void onCertsButtonClicked();
    void onContextMenu(const QPoint& pos);

private:
    void updateTreeMinimumHeight();
    std::string certFolderPath;
    std::vector<plugin::CertificateData> certificateList;

    QTreeWidget* treeWidget = nullptr;
    QTreeWidgetItem* tokenCertsItem = nullptr;
    QTreeWidgetItem* tokenPinItem = nullptr;
};

#endif // TOKENSECTION_H
