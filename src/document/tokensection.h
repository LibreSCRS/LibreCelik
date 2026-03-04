// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef TOKENSECTION_H
#define TOKENSECTION_H

#include <string>
#include <eidcard/eidtypes.h>
#include "utils/collapsiblesection.h"

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    explicit TokenSection(std::string certFolderPath, QWidget* parent = nullptr);

    void setPINVisible(bool visible);

public slots:
    void setCertificates(const eidcard::CertificateList& certList);
    void setPINStatus(int triesLeft, bool blocked);

signals:
    void changePINRequested();

private slots:
    void onCertsButtonClicked();
    void onContextMenu(const QPoint& pos);

private:
    std::string certFolderPath;
    eidcard::CertificateList certificateList;

    QPushButton* certsButton = nullptr;
    QPushButton* changePinButton = nullptr;
    QTreeWidget* treeWidget = nullptr;
    QTreeWidgetItem* tokenCertsItem = nullptr;
    QTreeWidgetItem* tokenPinItem = nullptr;
};

#endif // TOKENSECTION_H
