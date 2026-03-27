// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "tokensection.h"
#include "certificate/certificateviewerdlg.h"

#include <QBrush>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
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

constexpr int PinReferenceRole = Qt::UserRole;
constexpr int PinTransportRole = Qt::UserRole + 1;
constexpr int PinLabelRole = Qt::UserRole + 2;
constexpr int PinBlockedRole = Qt::UserRole + 3;
constexpr int PinMinLengthRole = Qt::UserRole + 4;
constexpr int PinMaxLengthRole = Qt::UserRole + 5;
constexpr int PinCanChangeRole = Qt::UserRole + 6;

} // namespace

TokenSection::TokenSection(std::string certFolderPath, QWidget* parent)
    : CollapsibleSection(qtTrId("lc-token-title"), parent), certFolderPath(std::move(certFolderPath))
{
    contentLayout = new QVBoxLayout(this);
    contentLayout->setSpacing(2);
    contentLayout->setContentsMargins(6, 4, 6, 4);

    // Token info banner — inserted at index 0 by setTokenInfo() when data arrives

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

    contentLayout->addWidget(treeWidget);

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

static QString formatSerialNumber(const plugin::CardField& field)
{
    bool printable =
        std::all_of(field.value.begin(), field.value.end(), [](uint8_t c) { return c >= 0x20 && c < 0x7F; });
    if (printable)
        return QString::fromStdString(field.asString());

    QStringList hexParts;
    for (uint8_t b : field.value)
        hexParts << QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    return hexParts.join(':');
}

void TokenSection::setTokenInfo(const plugin::CardFieldGroup& tokenGroup)
{
    if (tokenGroup.fields.empty())
        return;
    if (headerCard)
        return; // Already populated

    std::vector<LibreSCRS::HeaderField> headerFields;

    for (const auto& field : tokenGroup.fields) {
        if (field.value.empty())
            continue;

        QString labelText;
        if (field.key == "label")
            labelText = qtTrId("lc-pki-token-label");
        else if (field.key == "serial_number")
            labelText = qtTrId("lc-pki-token-serial");
        else if (field.key == "manufacturer")
            labelText = qtTrId("lc-pki-token-manufacturer");
        else
            labelText = QString::fromStdString(field.key);

        QString valueText =
            (field.key == "serial_number") ? formatSerialNumber(field) : QString::fromStdString(field.asString());

        headerFields.push_back({labelText, valueText});
    }

    if (headerFields.empty())
        return;

    headerCard =
        new LibreSCRS::CardHeaderCard(QIcon(":/images/certificate-icon.svg"), QSize(64, 64), headerFields, this);

    contentLayout->insertWidget(0, headerCard);
    updateTreeMinimumHeight();
}

void TokenSection::setCertificates(const std::vector<plugin::CertificateData>& certList)
{
    certificateList = certList;

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

void TokenSection::setPINList(const std::vector<plugin::PinStatusEntry>& pins)
{
    if (pins.empty()) {
        tokenPinItem->setHidden(true);
        return;
    }

    while (tokenPinItem->childCount() > 0)
        delete tokenPinItem->takeChild(0);

    for (const auto& pin : pins) {
        auto* item = new QTreeWidgetItem(tokenPinItem);
        item->setText(0, QString::fromStdString(pin.label));

        if (!pin.initialized) {
            item->setText(1, qtTrId("lc-eid-pin-transport"));
        } else if (pin.blocked) {
            item->setText(1, qtTrId("lc-eid-pin-blocked"));
            item->setForeground(1, QBrush(Qt::red));
        } else if (pin.triesLeft >= 0)
            item->setText(1, qtTrId("lc-eid-pin-tries-remaining").arg(pin.triesLeft));
        else
            item->setText(1, qtTrId("lc-eid-pin-unknown"));

        item->setData(0, PinReferenceRole, pin.reference);
        item->setData(0, PinTransportRole, !pin.initialized);
        item->setData(0, PinLabelRole, QString::fromStdString(pin.label));
        item->setData(0, PinBlockedRole, pin.blocked);
        item->setData(0, PinMinLengthRole, pin.minLength);
        item->setData(0, PinMaxLengthRole, pin.maxLength);
        item->setData(0, PinCanChangeRole, pin.canChange);
    }

    tokenPinItem->setHidden(false);
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

void TokenSection::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = treeWidget->itemAt(pos);
    if (!item)
        return;

    QMenu menu(this);

    bool isCertItem = (item->parent() == tokenCertsItem);
    bool isCertChild = (item->parent() && item->parent()->parent() == tokenCertsItem);
    if (isCertItem || isCertChild) {
        QTreeWidgetItem* certItem = isCertItem ? item : item->parent();
        int certIndex = tokenCertsItem->indexOfChild(certItem);
        QAction* viewAction = menu.addAction(qtTrId("lc-eid-menu-view-cert"));
        connect(viewAction, &QAction::triggered, this, [this, certIndex]() {
            if (certificateList.empty())
                return;
            auto dlg = std::make_unique<CertificateViewerDlg>(certificateList, certFolderPath, this, certIndex);
            dlg->exec();
        });
    }

    if (item->parent() == tokenPinItem) {
        bool canChange = item->data(0, PinCanChangeRole).toBool();
        if (!canChange)
            return;

        bool isTransport = item->data(0, PinTransportRole).toBool();
        bool isBlocked = item->data(0, PinBlockedRole).toBool();
        uint8_t pinRef = static_cast<uint8_t>(item->data(0, PinReferenceRole).toUInt());
        QString pinLabel = item->data(0, PinLabelRole).toString();

        QString actionText = isTransport ? qtTrId("lc-eid-menu-initialize-pin") : qtTrId("lc-eid-menu-change-pin");
        QAction* pinAction = menu.addAction(actionText);

        if (isBlocked) {
            pinAction->setEnabled(false);
        } else {
            connect(pinAction, &QAction::triggered, this, [this, pinRef, pinLabel, isTransport, item]() {
                int minLen = item->data(0, PinMinLengthRole).toInt();
                int maxLen = item->data(0, PinMaxLengthRole).toInt();
                emit changePINRequested(pinRef, pinLabel, isTransport, minLen, maxLen);
            });
        }
    }

    if (!menu.isEmpty())
        menu.exec(treeWidget->viewport()->mapToGlobal(pos));
}
