// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "tokensection.h"
#include "certificate/cert_format.h"
#include "certificate/certificateviewerdlg.h"
#ifdef LIBRECELIK_SIGNING_ENABLED
#include "signing/certutils.h"
#endif

#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <QBrush>
#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHeaderView>
#include <QIcon>
#include <QMenu>
#include <QPropertyAnimation>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStringList>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cstdint>

namespace lcc = LibreSCRS::Certificate;
namespace cf = librecelik::cert_format;

namespace {

struct CertInfo
{
    QString subject;
    QString issuer;
    QString algorithm;
    QString keyUsage;
    QString validFrom;
    QString validTo;
};

CertInfo parseCertInfo(const std::vector<uint8_t>& der)
{
    CertInfo info;
    auto cert = lcc::ParsedCertificate::fromDer(der);
    if (!cert)
        return info;

    info.subject = QString::fromStdString(cert->subject().commonName());
    info.issuer = QString::fromStdString(cert->issuer().commonName());
    if (info.issuer.isEmpty())
        info.issuer = qtTrId("lc-cert-unknown");
    info.algorithm = cf::publicKeyDescription(cert->publicKey());

    if (auto ku = cert->keyUsage())
        info.keyUsage = cf::keyUsageToStringEndEntity(*ku);

    info.validFrom = cf::formatDate(cert->notBefore());
    info.validTo = cf::formatDate(cert->notAfter());
    return info;
}

constexpr int PinReferenceRole = Qt::UserRole;
constexpr int PinTransportRole = Qt::UserRole + 1;
constexpr int PinLabelRole = Qt::UserRole + 2;
constexpr int PinBlockedRole = Qt::UserRole + 3;
constexpr int PinMinLengthRole = Qt::UserRole + 4;
constexpr int PinMaxLengthRole = Qt::UserRole + 5;
constexpr int PinCanChangeRole = Qt::UserRole + 6;

#ifdef LIBRECELIK_SIGNING_ENABLED
bool isSigningCapable(const LibreSCRS::Plugin::CertificateData& cert)
{
    // 4.0 ABI v6 made keyFID a std::optional<uint16_t>; a disengaged optional
    // (or an explicit 0, preserved from the internal 0-sentinel fallback in
    // some plugins) means "no associated private key known" → verify-only.
    if (!cert.keyFID || *cert.keyFID == 0)
        return false;

    auto parsed = lcc::ParsedCertificate::fromDer(cert.derBytes);
    if (!parsed)
        return false;

    auto ku = parsed->keyUsage();
    if (!ku)
        return false;

    for (auto bit : *ku) {
        if (bit == lcc::KeyUsageBit::DigitalSignature || bit == lcc::KeyUsageBit::NonRepudiation)
            return true;
    }
    return false;
}
#endif

} // namespace

TokenSection::TokenSection(std::shared_ptr<const LibreSCRS::Trust::TrustStore> store, QWidget* parent)
    : CollapsibleSection(QString(), parent), trustStore(std::move(store))
{
    contentLayout = new QVBoxLayout(this);
    contentLayout->setSpacing(2);
    contentLayout->setContentsMargins(6, 4, 6, 4);

    // Token info banner — inserted at index 0 by setTokenInfo() when data arrives

    treeWidget = new QTreeWidget(this);
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    treeWidget->setUniformRowHeights(true);
    treeWidget->header()->setStretchLastSection(true);
    treeWidget->header()->resizeSection(0, 230);
    treeWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    treeWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    treeWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    treeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    applyTreeStyleSheet();

    // Header labels and root items are populated empty here; their text
    // is supplied by retranslateUi() at the end of construction so that
    // LanguageChange paths share a single source of truth.
    tokenCertsItem = new QTreeWidgetItem(treeWidget);
    tokenCertsItem->setExpanded(true);
    tokenPinItem = new QTreeWidgetItem(treeWidget);
    tokenPinItem->setExpanded(true);
    tokenPinItem->setHidden(true);

    contentLayout->addWidget(treeWidget);

    connect(treeWidget, &QTreeWidget::customContextMenuRequested, this, &TokenSection::onContextMenu);

#ifdef LIBRECELIK_SIGNING_ENABLED
    {
        signBtn = new QToolButton(this);
        signBtn->setIcon(QIcon(QStringLiteral(":/images/sign-icon.svg")));
        signBtn->setIconSize(QSize(24, 24));
        signBtn->setAutoRaise(true);
        signBtn->setEnabled(false);
        auto* dimEffect = new QGraphicsOpacityEffect(signBtn);
        dimEffect->setOpacity(0.3);
        signBtn->setGraphicsEffect(dimEffect);
        signBtn->setVisible(true);
        addHeaderWidget(signBtn);

        connect(signBtn, &QToolButton::clicked, this, [this]() {
            auto certs = signingCertificates();
            if (certs.size() == 1)
                emit signRequested(certs.front(), readerName);
            else if (certs.size() > 1)
                showCertificateDropdown();
        });
    }
#endif

    setExpanded(false);

    // Apply initial translations (title, header columns, root-item
    // labels, signBtn tooltip).
    retranslateUi();

    connect(this, &CollapsibleSection::sectionExpanded, this, [this]() {
        for (QWidget* p = parentWidget(); p; p = p->parentWidget()) {
            if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                auto* vbar = sa->verticalScrollBar();
                int widgetBottom = this->mapTo(sa->widget(), QPoint(0, this->height())).y();
                int target = qBound(0, widgetBottom - sa->viewport()->height() + 8, vbar->maximum());
                if (target <= vbar->value())
                    return;
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

static QString formatSerialNumber(const LibreSCRS::Plugin::CardField& field)
{
    bool printable =
        std::all_of(field.value.begin(), field.value.end(), [](uint8_t c) { return c >= 0x20 && c < 0x7F; });
    if (printable) {
        if (auto text = field.textValue())
            return QString::fromStdString(*text);
    }

    QStringList hexParts;
    for (uint8_t b : field.value)
        hexParts << QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    return hexParts.join(':');
}

void TokenSection::setTokenInfo(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    if (group.fields.empty())
        return;

    // Cache the raw group so retranslateUi() can rebuild the header card
    // with the current translator (per LM 4.0 retranslate pattern). The
    // first call populates the header; subsequent calls (e.g. invoked from
    // retranslateUi) clear and rebuild.
    tokenGroup = group;
    hasTokenGroup = true;

    if (headerCard) {
        contentLayout->removeWidget(headerCard);
        headerCard->deleteLater();
        headerCard = nullptr;
    }

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

        QString valueText;
        if (field.key == "serial_number") {
            valueText = formatSerialNumber(field);
        } else if (auto text = field.textValue()) {
            valueText = QString::fromStdString(*text);
        }

        headerFields.push_back({labelText, valueText});
    }

    if (headerFields.empty())
        return;

    headerCard = new LibreSCRS::CardHeaderCard(QIcon(QStringLiteral(":/images/certificate-icon.svg")), QSize(64, 64),
                                               headerFields, this);

    contentLayout->insertWidget(0, headerCard);
    updateTreeMinimumHeight();
}

void TokenSection::setCertificates(const std::vector<LibreSCRS::Plugin::CertificateData>& certList)
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
        addRow(qtTrId("lc-cert-field-issuer"), info.issuer);
        addRow(qtTrId("lc-token-key-algorithm"), info.algorithm);
        addRow(qtTrId("lc-token-key-usage"), info.keyUsage);

        QString validity;
        if (!info.validFrom.isEmpty() && !info.validTo.isEmpty())
            validity = info.validFrom + " – " + info.validTo;
        addRow(qtTrId("lc-token-key-valid"), validity);

        if (cert.keyFID && *cert.keyFID != 0)
            addRow(qtTrId("lc-token-key-private-key"), "✓");
    }

#ifdef LIBRECELIK_SIGNING_ENABLED
    bool canSign = !signingCertificates().empty();
    if (canSign) {
        if (signBtn->graphicsEffect())
            signBtn->setGraphicsEffect(nullptr);
    } else if (!signBtn->graphicsEffect()) {
        auto* dimEffect = new QGraphicsOpacityEffect(signBtn);
        dimEffect->setOpacity(0.3);
        signBtn->setGraphicsEffect(dimEffect);
    }
    signBtn->setEnabled(canSign);
#endif

    updateTreeMinimumHeight();
}

void TokenSection::setPINList(const std::vector<LibreSCRS::Plugin::PinStatusEntry>& pins)
{
    pinList = pins;

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
        } else if (pin.retriesLeft.has_value())
            item->setText(1, qtTrId("lc-eid-pin-tries-remaining").arg(*pin.retriesLeft));
        else
            item->setText(1, qtTrId("lc-eid-pin-unknown"));

        item->setData(0, PinReferenceRole, pin.reference);
        item->setData(0, PinTransportRole, !pin.initialized);
        item->setData(0, PinLabelRole, QString::fromStdString(pin.label));
        item->setData(0, PinBlockedRole, pin.blocked);
        item->setData(0, PinMinLengthRole, pin.minLength.has_value() ? static_cast<int>(*pin.minLength) : 0);
        item->setData(0, PinMaxLengthRole, pin.maxLength.has_value() ? static_cast<int>(*pin.maxLength) : 0);
        item->setData(0, PinCanChangeRole, pin.canChange);
    }

    tokenPinItem->setHidden(false);
    tokenPinItem->setExpanded(true);
    updateTreeMinimumHeight();
}

void TokenSection::updateTreeMinimumHeight()
{
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
        QAction* viewAction =
            menu.addAction(qtTrId("lc-eid-menu-view-cert")); // i18n-audit: ignore D2, transient context-menu — qtTrId
                                                             // evaluated per right-click; menu discarded after exec()
        connect(viewAction, &QAction::triggered, this, [this, certIndex]() {
            if (certificateList.empty())
                return;
            auto dlg = std::make_unique<CertificateViewerDlg>(certificateList, trustStore, this, certIndex);
            dlg->exec();
        });

#ifdef LIBRECELIK_SIGNING_ENABLED
        if (certIndex >= 0 && certIndex < static_cast<int>(certificateList.size()) &&
            isSigningCapable(certificateList[static_cast<size_t>(certIndex)])) {
            QAction* signAction = menu.addAction(qtTrId("lc-sign-with-cert"));
            connect(signAction, &QAction::triggered, this, [this, certIndex]() {
                emit signRequested(certificateList[static_cast<size_t>(certIndex)], readerName);
            });
        }
#endif
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

#ifdef LIBRECELIK_SIGNING_ENABLED
void TokenSection::setReaderName(const std::string& name)
{
    readerName = name;
}

std::vector<LibreSCRS::Plugin::CertificateData> TokenSection::signingCertificates() const
{
    std::vector<LibreSCRS::Plugin::CertificateData> result;
    for (const auto& cert : certificateList) {
        if (isSigningCapable(cert))
            result.push_back(cert);
    }
    return result;
}

void TokenSection::showCertificateDropdown()
{
    auto certs = signingCertificates();
    if (certs.empty())
        return;

    QMenu menu(this);
    for (const auto& cert : certs) {
        QString label = QString::fromStdString(cert.label);
        QString cn = signing::subjectCN(cert.derBytes);
        QString text = cn.isEmpty() ? label : label + " (" + cn + ")";
        QAction* action = menu.addAction(text);
        connect(action, &QAction::triggered, this, [this, cert]() { emit signRequested(cert, readerName); });
    }
    QSize menuSize = menu.sizeHint();
    QPoint pos = signBtn->mapToGlobal(QPoint(signBtn->width() - menuSize.width(), signBtn->height()));
    QRect screenGeo = signBtn->screen()->availableGeometry();
    if (pos.x() < screenGeo.left())
        pos.setX(screenGeo.left());
    if (pos.x() + menuSize.width() > screenGeo.right())
        pos.setX(screenGeo.right() - menuSize.width());
    menu.exec(pos);
}
#endif

void TokenSection::applyTreeStyleSheet()
{
    QString textColor = palette().color(QPalette::HighlightedText).name();
    treeWidget->setStyleSheet(QString("QTreeView::item:selected {"
                                      "  background-color: rgb(34, 86, 117);"
                                      "  color: %1;"
                                      "}"
                                      "QTreeView::item:selected:!active {"
                                      "  background-color: rgb(34, 86, 117);"
                                      "  color: %1;"
                                      "}")
                                  .arg(textColor));
}

void TokenSection::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        applyTreeStyleSheet();
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    CollapsibleSection::changeEvent(event);
}

void TokenSection::retranslateUi()
{
    setTitle(qtTrId("lc-token-title"));
    treeWidget->setHeaderLabels({qtTrId("lc-token-col-object"), qtTrId("lc-token-col-details")});

    if (tokenCertsItem)
        tokenCertsItem->setText(0, qtTrId("lc-eid-tree-certificates"));
    if (tokenPinItem)
        tokenPinItem->setText(0, qtTrId("lc-eid-tree-pin"));

#ifdef LIBRECELIK_SIGNING_ENABLED
    if (signBtn)
        signBtn->setToolTip(qtTrId("lc-sign-button"));
#endif

    if (hasTokenGroup)
        setTokenInfo(tokenGroup);
    setCertificates(certificateList);
    setPINList(pinList);
}
