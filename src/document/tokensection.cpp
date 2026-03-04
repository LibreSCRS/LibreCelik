// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "tokensection.h"
#include "certificate/certificateviewerdlg.h"

#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

TokenSection::TokenSection(std::string certFolderPath, QWidget* parent)
    : CollapsibleSection(qtTrId("lc-token-title"), parent)
    , certFolderPath(std::move(certFolderPath))
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(6, 4, 6, 4);

    // Buttons go into the header bar
    certsButton = new QPushButton(qtTrId("lc-token-certs-button"));
    certsButton->setIcon(QIcon(":/images/certificate-icon.png"));
    certsButton->setEnabled(false);
    changePinButton = new QPushButton(qtTrId("lc-token-change-pin"));
    changePinButton->setIcon(QIcon(":/images/pin-change-icon.png"));
    addHeaderWidget(certsButton);
    addHeaderWidget(changePinButton);

    // Tree widget
    treeWidget = new QTreeWidget(this);
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    treeWidget->setUniformRowHeights(true);
    treeWidget->setHeaderLabels({tr("Object"), tr("Details")});
    treeWidget->header()->setStretchLastSection(true);
    treeWidget->header()->resizeSection(0, 200);

    tokenCertsItem = new QTreeWidgetItem(treeWidget, QStringList{qtTrId("lc-eid-tree-certificates")});
    tokenCertsItem->setExpanded(true);
    tokenPinItem = new QTreeWidgetItem(treeWidget, QStringList{qtTrId("lc-eid-tree-pin")});
    tokenPinItem->setExpanded(true);
    tokenPinItem->setHidden(true);

    layout->addWidget(treeWidget);

    connect(certsButton, &QPushButton::clicked, this, &TokenSection::onCertsButtonClicked);
    connect(changePinButton, &QPushButton::clicked, this, &TokenSection::changePINRequested);
    connect(treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &TokenSection::onContextMenu);

    setExpanded(false);
}

void TokenSection::setPINVisible(bool visible)
{
    tokenPinItem->setHidden(!visible);
}

void TokenSection::setCertificates(const eidcard::CertificateList& certList)
{
    certificateList = certList;
    certsButton->setEnabled(!certificateList.empty());

    while (tokenCertsItem->childCount() > 0)
        delete tokenCertsItem->takeChild(0);

    for (const auto& cert : certificateList) {
        auto* item = new QTreeWidgetItem(tokenCertsItem);
        item->setText(0, QString::fromStdString(cert.label));
        item->setText(1, cert.keyFID != 0 ? qtTrId("lc-eid-cert-can-sign") : QString());
    }
}

void TokenSection::setPINStatus(int triesLeft, bool blocked)
{
    // triesLeft == -2 means card type has no PIN (e.g. Apollo eID)
    if (triesLeft == -2) {
        tokenPinItem->setHidden(true);
        return;
    }

    while (tokenPinItem->childCount() > 0)
        delete tokenPinItem->takeChild(0);

    auto* item = new QTreeWidgetItem(tokenPinItem);
    item->setText(0, qtTrId("lc-eid-pin-user"));
    if (blocked)
        item->setText(1, qtTrId("lc-eid-pin-blocked"));
    else if (triesLeft >= 0)
        item->setText(1, qtTrId("lc-eid-pin-tries-remaining").arg(triesLeft));
    else
        item->setText(1, qtTrId("lc-eid-pin-unknown"));
    tokenPinItem->setExpanded(true);
}

void TokenSection::onCertsButtonClicked()
{
    if (certificateList.empty())
        return;

    auto dlg = std::make_unique<CertificateViewerDlg>(certificateList, certFolderPath, this);
    dlg->exec();
}

void TokenSection::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = treeWidget->itemAt(pos);
    if (!item)
        return;

    QMenu menu(this);

    // Certificate child item → "View Certificate"
    if (item->parent() == tokenCertsItem) {
        QAction* viewAction = menu.addAction(qtTrId("lc-eid-menu-view-cert"));
        connect(viewAction, &QAction::triggered, this, &TokenSection::onCertsButtonClicked);
    }

    // User PIN child item → "Change PIN"
    if (item->parent() == tokenPinItem) {
        QAction* pinAction = menu.addAction(qtTrId("lc-eid-menu-change-pin"));
        connect(pinAction, &QAction::triggered, this, &TokenSection::changePINRequested);
    }

    if (!menu.isEmpty())
        menu.exec(treeWidget->viewport()->mapToGlobal(pos));
}
