// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "tokensection.h"

#include "certificate/certfields.h"
#include "certificate/certformat.h"

#include <QBrush>
#include <QDateTime>
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

#include <optional>

namespace {

using LibreSCRS::AgentClient::CertificateInfo;
using LibreSCRS::AgentClient::CredentialKind;
using LibreSCRS::AgentClient::CredentialRecord;
using LibreSCRS::AgentClient::CredentialState;
using LibreSCRS::AgentClient::TrustStatus;

/// Index of the credential a tree row renders. The row carries nothing else:
/// every value the context menu acts on lives on the record itself, so one
/// look-up beats mirroring half a dozen members into item data roles.
constexpr int CredentialIndexRole = Qt::UserRole;

/// Compact validity date — the format the token summary has always used.
[[nodiscard]] QString formatValidityDate(const QDateTime& moment)
{
    return moment.isValid() ? moment.toUTC().toString(QStringLiteral("dd.MM.yyyy")) : QString();
}

/// One-line description of the key a certificate carries:
/// `"<algorithm> <bits>-bit"`, with the curve appended in parentheses when the
/// certificate names one — the shape this summary has always shown. Built from
/// the agent's public-key group; empty when the agent resolved no algorithm,
/// which drops the row rather than printing a bare bit count.
[[nodiscard]] QString publicKeyDescription(const CertificateInfo& cert)
{
    namespace fields = librecelik::certfields;
    const QVariantMap groups = fields::groupsOf(cert);
    const QString algorithm = fields::valueOf(groups, QLatin1StringView("publicKey"), QLatin1StringView("algorithm"));
    if (algorithm.isEmpty())
        return {};

    QString description = algorithm;
    const QString sizeBits = fields::valueOf(groups, QLatin1StringView("publicKey"), QLatin1StringView("sizeBits"));
    if (!sizeBits.isEmpty())
        description += QStringLiteral(" %1-bit").arg(sizeBits);
    const QString curve = fields::valueOf(groups, QLatin1StringView("publicKey"), QLatin1StringView("curveOid"));
    if (!curve.isEmpty())
        description += QStringLiteral(" (%1)").arg(curve);
    return description;
}

/// LC copy for one security-status token.
///
/// Same known-token/verbatim rule the agent error line uses: a token this
/// build names renders LC's own string, anything else is displayed exactly as
/// it arrived. The vocabulary is the agent's and grows independently of this
/// build, so an unknown token is forward-compatible display data — showing it
/// raw beats swallowing a verdict a newer agent considers worth reporting.
[[nodiscard]] QString securityStatusText(const QString& token)
{
    if (token == QLatin1StringView("trusted"))
        return qtTrId("lc-cert-status-trusted");
    if (token == QLatin1StringView("untrusted-root"))
        return qtTrId("lc-cert-status-untrusted-root");
    if (token == QLatin1StringView("broken-chain"))
        return qtTrId("lc-cert-status-broken-chain");
    if (token == QLatin1StringView("invalid"))
        return qtTrId("lc-cert-status-invalid");
    if (token == QLatin1StringView("expired"))
        return qtTrId("lc-cert-status-expired");
    if (token == QLatin1StringView("revoked"))
        return qtTrId("lc-cert-status-revoked");
    if (token == QLatin1StringView("offline-unverified"))
        return qtTrId("lc-cert-status-offline-unverified");
    return token;
}

/// The status column of a certificate row: every token the agent reported,
/// in the order it reported them.
[[nodiscard]] QString certificateStatusText(const CertificateInfo& cert)
{
    QStringList parts;
    parts.reserve(cert.securityStatus.size());
    for (const QString& token : cert.securityStatus)
        parts << securityStatusText(token);
    return parts.join(QStringLiteral(" · "));
}

/// Badge icon for a trust verdict. Unknown is deliberately unadorned: it means
/// no verdict was available, not a negative one, and a marker on every row
/// would read as one.
[[nodiscard]] QString trustIconPath(TrustStatus trust)
{
    switch (trust) {
    case TrustStatus::Trusted:
        return QStringLiteral(":/images/green_checked.png");
    case TrustStatus::Untrusted:
    case TrustStatus::Revoked:
    case TrustStatus::Expired:
        return QStringLiteral(":/images/red_exclamation.png");
    case TrustStatus::Unknown:
        break;
    }
    return {};
}

/// Colour for the same verdict, on the same reasoning as @ref trustIconPath.
[[nodiscard]] std::optional<Qt::GlobalColor> trustColor(TrustStatus trust)
{
    switch (trust) {
    case TrustStatus::Trusted:
        return Qt::darkGreen;
    case TrustStatus::Untrusted:
    case TrustStatus::Revoked:
    case TrustStatus::Expired:
        return Qt::red;
    case TrustStatus::Unknown:
        break;
    }
    return std::nullopt;
}

/// Counter fragments of a credential row, in the order they are shown.
[[nodiscard]] QStringList credentialCounters(const CredentialRecord& cred)
{
    QStringList parts;
    if (cred.retriesLeft.has_value())
        parts << (cred.retriesMax.has_value()
                      ? qtTrId("lc-eid-pin-tries-remaining-of").arg(*cred.retriesLeft).arg(*cred.retriesMax)
                      : qtTrId("lc-eid-pin-tries-remaining").arg(*cred.retriesLeft));
    if (cred.usesLeft.has_value())
        parts << (cred.usesMax.has_value()
                      ? qtTrId("lc-eid-pin-uses-remaining-of").arg(*cred.usesLeft).arg(*cred.usesMax)
                      : qtTrId("lc-eid-pin-uses-remaining").arg(*cred.usesLeft));
    if (cred.unblocksLeft.has_value())
        parts << qtTrId("lc-eid-pin-unblocks-remaining").arg(*cred.unblocksLeft);
    return parts;
}

/// Holder guidance for a blocked credential.
///
/// The known-key/fallback rule of the agent error line: a guidance key this
/// build named would render LC's own copy first. LC names none — the key
/// vocabulary belongs to the agent and grows independently of this build — so
/// the agent's authored fallback is what a holder reads, and a key that
/// arrives without one renders nothing rather than a guess made on this side.
[[nodiscard]] QString blockedGuidanceText(const CredentialRecord& cred)
{
    const QString fallback = cred.blockedGuidanceFallback.value_or(QString());
    return fallback.trimmed().isEmpty() ? QString() : fallback;
}

/// The status column of a credential row: lifecycle badge first, then whatever
/// counters the card reported.
[[nodiscard]] QString credentialStatusText(const CredentialRecord& cred)
{
    if (cred.state == CredentialState::Transport)
        return qtTrId("lc-eid-pin-transport");

    if (cred.state == CredentialState::Blocked) {
        const QString guidance = blockedGuidanceText(cred);
        return guidance.isEmpty() ? qtTrId("lc-eid-pin-blocked")
                                  : qtTrId("lc-eid-pin-blocked") + QStringLiteral(" · ") + guidance;
    }

    QStringList parts = credentialCounters(cred);
    if (cred.state == CredentialState::NeedsChange)
        parts.prepend(qtTrId("lc-eid-pin-needs-change"));
    if (!parts.isEmpty())
        return parts.join(QStringLiteral(" · "));

    // A CAN is a printed access number with no retry/usage state — show a
    // blank status rather than the "?" unknown marker (which reads as an
    // error). Every other counter-less credential keeps the "?".
    return cred.kind == CredentialKind::Can ? QString() : qtTrId("lc-eid-pin-unknown");
}

} // namespace

namespace librecelik::document {

QString formatFieldOrPlaceholder(const QString& value)
{
    // U+2014 EM DASH. Use the universal-character-name escape (not raw
    // \xe2\x80\x94 bytes) because QStringLiteral treats each \x escape as
    // a Latin-1 QChar, which would yield three U+00E2/U+0080/U+0094 chars
    // and a six-byte UTF-8 round-trip instead of the intended single
    // em-dash. \u escapes feed a true code point to the string-pool.
    return value.isEmpty() ? QStringLiteral("—") : value;
}

} // namespace librecelik::document

TokenSection::TokenSection(QWidget* parent) : CollapsibleSection(QString(), parent)
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
                emit signRequested(certs.front(), cardId);
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

void TokenSection::setCardId(const QString& id)
{
    cardId = id;
}

void TokenSection::setTokenInfo(const LibreSCRS::AgentClient::FieldGroup& group)
{
    // Cache the raw group so retranslateUi() can rebuild the header card
    // with the current translator. The first call populates the header;
    // subsequent calls (e.g. invoked from retranslateUi) clear and rebuild.
    tokenGroup = group;
    hasTokenGroup = true;

    if (headerCard) {
        contentLayout->removeWidget(headerCard);
        headerCard->deleteLater();
        headerCard = nullptr;
    }

    std::vector<librecelik::utils::HeaderField> headerFields;

    for (const auto& field : tokenGroup.fields) {
        QString labelText;
        if (field.key == QLatin1StringView("label"))
            labelText = qtTrId("lc-pki-token-label");
        else if (field.key == QLatin1StringView("serial_number"))
            labelText = qtTrId("lc-pki-token-serial");
        else if (field.key == QLatin1StringView("manufacturer"))
            labelText = qtTrId("lc-pki-token-manufacturer");
        else
            labelText = field.key;

        headerFields.push_back({labelText, librecelik::document::formatFieldOrPlaceholder(field.value)});
    }

    // Empty-group fallback: render the three canonical token rows with
    // em-dash placeholders so the header is always structurally present.
    // This is the load-bearing recovery path when the card read returned no
    // token fields at all.
    if (headerFields.empty()) {
        const QString placeholder = librecelik::document::formatFieldOrPlaceholder(QString());
        headerFields.push_back({qtTrId("lc-pki-token-label"), placeholder});
        headerFields.push_back({qtTrId("lc-pki-token-serial"), placeholder});
        headerFields.push_back({qtTrId("lc-pki-token-manufacturer"), placeholder});
    }

    headerCard = new librecelik::utils::CardHeaderCard(QIcon(QStringLiteral(":/images/certificate-icon.svg")),
                                                       QSize(64, 64), headerFields, this);

    contentLayout->insertWidget(0, headerCard);
    updateTreeMinimumHeight();
}

void TokenSection::setCertificates(const QList<LibreSCRS::AgentClient::CertificateInfo>& certs)
{
    certificateList = certs;

    while (tokenCertsItem->childCount() > 0)
        delete tokenCertsItem->takeChild(0);

    for (const auto& cert : certificateList) {
        auto* certItem = new QTreeWidgetItem(tokenCertsItem);
        // The subject names the certificate in the tree; the opaque id is the
        // only thing left to show when the agent reported no subject at all.
        certItem->setText(0, cert.subject.isEmpty() ? cert.id : cert.subject);
        certItem->setText(1, certificateStatusText(cert));
        if (const QString iconPath = trustIconPath(cert.trust); !iconPath.isEmpty())
            certItem->setIcon(0, QIcon(iconPath));
        if (const auto color = trustColor(cert.trust))
            certItem->setForeground(1, QBrush(*color));
        certItem->setExpanded(true);

        auto addRow = [&](const QString& label, const QString& value) {
            if (value.isEmpty())
                return;
            auto* row = new QTreeWidgetItem(certItem);
            row->setText(0, label);
            row->setText(1, value);
        };

        addRow(qtTrId("lc-cert-field-issuer"), cert.issuer);
        addRow(qtTrId("lc-token-key-algorithm"), publicKeyDescription(cert));
        // End-entity capability only: the CA and cipher-direction bits belong
        // to the certificate viewer, which shows the complete list.
        addRow(qtTrId("lc-token-key-usage"), librecelik::certformat::keyUsageToStringEndEntity(cert.keyUsageBits));

        const QString validFrom = formatValidityDate(cert.notBefore);
        const QString validTo = formatValidityDate(cert.notAfter);
        QString validity;
        if (!validFrom.isEmpty() && !validTo.isEmpty())
            validity = validFrom + " – " + validTo;
        addRow(qtTrId("lc-token-key-valid"), validity);

        if (cert.signingCapable)
            addRow(qtTrId("lc-token-key-private-key"), "✓");
    }

#ifdef LIBRECELIK_SIGNING_ENABLED
    // The button is live exactly when this card offers a certificate that can
    // sign; dimmed and disabled otherwise, never hidden.
    const bool canSign = !signingCertificates().empty();
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

void TokenSection::setCredentials(const LibreSCRS::AgentClient::CredentialList& credentials)
{
    credentialList = credentials;

    if (credentialList.isEmpty()) {
        tokenPinItem->setHidden(true);
        return;
    }

    while (tokenPinItem->childCount() > 0)
        delete tokenPinItem->takeChild(0);

    for (qsizetype index = 0; index < credentialList.size(); ++index) {
        const CredentialRecord& cred = credentialList.at(index);

        auto* item = new QTreeWidgetItem(tokenPinItem);
        // The agent authors the label; the opaque id is the only honest
        // fallback when it authored none.
        item->setText(0, cred.label.isEmpty() ? cred.id : cred.label);
        item->setText(1, credentialStatusText(cred));
        if (cred.state == CredentialState::Blocked)
            item->setForeground(1, QBrush(Qt::red));
        item->setData(0, CredentialIndexRole, QVariant::fromValue(index));
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
        if (certIndex >= 0 && certIndex < static_cast<int>(certificateList.size())) {
            QAction* viewAction =
                menu.addAction(qtTrId("lc-eid-menu-view-cert")); // i18n-audit: ignore D2, transient context-menu —
                                                                 // qtTrId evaluated per right-click; menu discarded
                                                                 // after exec()
            connect(viewAction, &QAction::triggered, this,
                    [this, certIndex]() { emit certificateDetailsRequested(certificateList.at(certIndex)); });

#ifdef LIBRECELIK_SIGNING_ENABLED
            if (certificateList.at(certIndex).signingCapable) {
                QAction* signAction = menu.addAction(qtTrId("lc-sign-with-cert"));
                connect(signAction, &QAction::triggered, this,
                        [this, certIndex]() { emit signRequested(certificateList.at(certIndex), cardId); });
            }
#endif
        }
    }

    if (item->parent() == tokenPinItem) {
        const qsizetype credIndex = item->data(0, CredentialIndexRole).value<qsizetype>();
        if (credIndex < 0 || credIndex >= credentialList.size())
            return;

        const CredentialRecord& cred = credentialList.at(credIndex);
        if (!cred.canChange)
            return;

        const bool isTransport = (cred.state == CredentialState::Transport);
        QString actionText = isTransport ? qtTrId("lc-eid-menu-initialize-pin") : qtTrId("lc-eid-menu-change-pin");
        QAction* pinAction = menu.addAction(actionText);

        // TEMP(C1-rewire): pin side re-enabled in Task 27 (the agent-side
        // change-PIN dialog installs the changePinRequested connect and
        // retires this tooltip). A blocked credential was already disabled
        // here; every credential is, for as long as there is no dialog to
        // launch — and the tooltip says so rather than leaving a dead entry.
        pinAction->setEnabled(false);
        if (cred.state != CredentialState::Blocked) {
            pinAction->setToolTip( // i18n-audit: ignore D8, transient action rebuilt per right-click
                qtTrId("lc-agent-action-pending-rewire"));
            menu.setToolTipsVisible(true);
        }
    }

    if (!menu.isEmpty())
        menu.exec(treeWidget->viewport()->mapToGlobal(pos));
}

#ifdef LIBRECELIK_SIGNING_ENABLED
QList<LibreSCRS::AgentClient::CertificateInfo> TokenSection::signingCertificates() const
{
    QList<LibreSCRS::AgentClient::CertificateInfo> result;
    for (const auto& cert : certificateList) {
        if (cert.signingCapable)
            result.append(cert);
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
        QString text = cert.subject.isEmpty() ? cert.id : cert.subject;
        QAction* action = menu.addAction(text);
        connect(action, &QAction::triggered, this, [this, cert]() { emit signRequested(cert, cardId); });
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
    setCredentials(credentialList);
}
