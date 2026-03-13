// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "tokensection.h"
#include "certificate/certificateviewerdlg.h"

#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <openssl/evp.h>
#include <openssl/x509v3.h>
#include <cstdint>
#include <ctime>

namespace {

struct CertInfo
{
    QString subject;
    QString algorithm;
    QString keyUsage;
    QString validFrom;
    QString validTo;
};

static QString asnTimeToDate(const ASN1_TIME* t)
{
    if (!t)
        return {};
    struct tm tm = {};
    if (ASN1_TIME_to_tm(t, &tm) != 1)
        return {};
    char buf[16];
    std::strftime(buf, sizeof(buf), "%d.%m.%Y", &tm);
    return QString::fromLatin1(buf);
}

static CertInfo parseCertInfo(const std::vector<uint8_t>& der)
{
    CertInfo info;
    const uint8_t* p = der.data();
    X509* cert = d2i_X509(nullptr, &p, static_cast<long>(der.size()));
    if (!cert)
        return info;

    // Subject CN
    X509_NAME* subject = X509_get_subject_name(cert);
    int cnIdx = X509_NAME_get_index_by_NID(subject, NID_commonName, -1);
    if (cnIdx >= 0) {
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(subject, cnIdx);
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        unsigned char* utf8 = nullptr;
        int len = ASN1_STRING_to_UTF8(&utf8, data);
        if (len > 0) {
            info.subject = QString::fromUtf8(reinterpret_cast<char*>(utf8), len);
            OPENSSL_free(utf8);
        }
    }

    // Algorithm and key size
    EVP_PKEY* pkey = X509_get0_pubkey(cert);
    if (pkey) {
        int id = EVP_PKEY_base_id(pkey);
        int bits = EVP_PKEY_bits(pkey);
        const char* name = OBJ_nid2sn(id);
        info.algorithm = QString("%1 %2-bit").arg(name ? QString::fromLatin1(name) : "?").arg(bits);
    }

    // Key usage
    uint32_t usage = X509_get_key_usage(cert);
    if (usage != ~uint32_t{0}) {
        QStringList usages;
        if (usage & KU_DIGITAL_SIGNATURE)
            usages << qtTrId("lc-token-ku-digital-signature");
        if (usage & KU_NON_REPUDIATION)
            usages << qtTrId("lc-token-ku-non-repudiation");
        if (usage & KU_KEY_ENCIPHERMENT)
            usages << qtTrId("lc-token-ku-key-encipherment");
        if (usage & KU_DATA_ENCIPHERMENT)
            usages << qtTrId("lc-token-ku-data-encipherment");
        if (usage & KU_KEY_AGREEMENT)
            usages << qtTrId("lc-token-ku-key-agreement");
        info.keyUsage = usages.join(", ");
    }

    // Validity
    info.validFrom = asnTimeToDate(X509_get0_notBefore(cert));
    info.validTo = asnTimeToDate(X509_get0_notAfter(cert));

    X509_free(cert);
    return info;
}

} // namespace

TokenSection::TokenSection(std::string certFolderPath, QWidget* parent)
    : CollapsibleSection(qtTrId("lc-token-title"), parent), certFolderPath(std::move(certFolderPath))
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
    treeWidget->setHeaderLabels({qtTrId("lc-token-col-object"), qtTrId("lc-token-col-details")});
    treeWidget->header()->setStretchLastSection(true);
    treeWidget->header()->resizeSection(0, 230);
    // No internal scrollbars — tree always shows full content;
    // outer scroll area handles overflow.
    treeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    treeWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    treeWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    treeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    treeWidget->setStyleSheet("QTreeView::item:selected {"
                              "  background-color: rgb(34, 86, 117);"
                              "  color: white;"
                              "}"
                              "QTreeView::item:selected:!active {"
                              "  background-color: rgb(34, 86, 117);"
                              "  color: white;"
                              "}");

    tokenCertsItem = new QTreeWidgetItem(treeWidget, QStringList{qtTrId("lc-eid-tree-certificates")});
    tokenCertsItem->setExpanded(true);
    tokenPinItem = new QTreeWidgetItem(treeWidget, QStringList{qtTrId("lc-eid-tree-pin")});
    tokenPinItem->setExpanded(true);
    tokenPinItem->setHidden(true);

    layout->addWidget(treeWidget);

    connect(certsButton, &QPushButton::clicked, this, &TokenSection::onCertsButtonClicked);
    connect(changePinButton, &QPushButton::clicked, this, &TokenSection::changePINRequested);
    connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &TokenSection::onContextMenu);

    setExpanded(false);

    // When this section finishes expanding, smoothly scroll the outer scroll area
    // to reveal it (matching the 200ms CollapsibleSection animation duration).
    connect(this, &CollapsibleSection::sectionExpanded, this, [this]() {
        for (QWidget* p = parentWidget(); p; p = p->parentWidget()) {
            if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                auto* vbar = sa->verticalScrollBar();
                int widgetBottom = this->mapTo(sa->widget(), QPoint(0, this->height())).y();
                int target = qBound(0, widgetBottom - sa->viewport()->height() + 8, vbar->maximum());
                if (target <= vbar->value())
                    return; // already fully visible
                auto* anim = new QPropertyAnimation(vbar, "value", sa);
                anim->setDuration(200);
                anim->setStartValue(vbar->value());
                anim->setEndValue(target);
                anim->setEasingCurve(QEasingCurve::OutCubic);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
                return;
            }
        }
    });
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
        auto* certItem = new QTreeWidgetItem(tokenCertsItem);
        certItem->setText(0, QString::fromStdString(cert.label));
        certItem->setExpanded(true);

        CertInfo info = parseCertInfo(cert.derBytes);

        auto addRow = [&](const QString& label, const QString& value) {
            if (value.isEmpty())
                return;
            auto* row = new QTreeWidgetItem(certItem);
            row->setText(0, label);
            row->setText(1, value);
        };

        addRow(qtTrId("lc-token-key-subject"), info.subject);
        addRow(qtTrId("lc-token-key-algorithm"), info.algorithm);
        addRow(qtTrId("lc-token-key-usage"), info.keyUsage);

        QString validity;
        if (!info.validFrom.isEmpty() && !info.validTo.isEmpty())
            validity = info.validFrom + " \u2013 " + info.validTo;
        addRow(qtTrId("lc-token-key-valid"), validity);

        if (cert.keyFID != 0)
            addRow(qtTrId("lc-token-key-private-key"), "\u2713");
    }
    updateTreeMinimumHeight();
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
    updateTreeMinimumHeight();
}

void TokenSection::updateTreeMinimumHeight()
{
    // With widgetResizable=true, outer QScrollArea uses minimumSizeHint() (not
    // sizeHint()) to decide whether to show scrollbars. Force the tree's minimum
    // height to its full content height so the constraint propagates up through
    // the layout chain and the outer scroll area shows a scrollbar when needed.
    treeWidget->setMinimumHeight(treeWidget->sizeHint().height());
    updateGeometry();
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

    // Certificate item or any of its sub-info children → "View Certificate"
    bool isCertItem = (item->parent() == tokenCertsItem);
    bool isCertChild = (item->parent() && item->parent()->parent() == tokenCertsItem);
    if (isCertItem || isCertChild) {
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
