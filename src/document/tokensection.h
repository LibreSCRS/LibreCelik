// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <string>
#include <vector>
#include <plugin/card_plugin.h>
#include "utils/collapsiblesection.h"
#include "utils/cardheadercard.h"

class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    explicit TokenSection(std::string certFolderPath, QWidget* parent = nullptr);

public slots:
    void setTokenInfo(const plugin::CardFieldGroup& tokenGroup);
    void setCertificates(const std::vector<plugin::CertificateData>& certList);
    void setPINList(const std::vector<plugin::PinStatusEntry>& pins);

signals:
    void changePINRequested(uint8_t pinReference, const QString& pinLabel, bool isTransport, int minLength,
                            int maxLength);

private slots:
    void onContextMenu(const QPoint& pos);

private:
    void updateTreeMinimumHeight();
    std::string certFolderPath;
    std::vector<plugin::CertificateData> certificateList;

    QVBoxLayout* contentLayout = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
    QTreeWidget* treeWidget = nullptr;
    QTreeWidgetItem* tokenCertsItem = nullptr;
    QTreeWidgetItem* tokenPinItem = nullptr;
};
