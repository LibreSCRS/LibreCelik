// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "config.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/Types.h>

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"

#include <QList>
#include <QString>

class QEvent;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

namespace librecelik::document {

/// @brief Returns @p value if non-empty, otherwise the em-dash placeholder
///        @c "—" (U+2014).
///
/// Used by @ref TokenSection to render token header fields the card read could
/// not populate (partial card data). The header card is structural UX and must
/// render even when individual values are missing; the placeholder makes the
/// gap visually explicit instead of silently producing a blank cell.
[[nodiscard]] QString formatFieldOrPlaceholder(const QString& value);

} // namespace librecelik::document

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    explicit TokenSection(QWidget* parent = nullptr);

    /// @brief Bind the card whose objects this section renders.
    ///
    /// The id is opaque and travels back out unchanged on @ref signRequested,
    /// so whoever launches the signing flow can address the same card without
    /// this widget knowing what the string denotes.
    void setCardId(const QString& cardId);

public slots:
    void setTokenInfo(const LibreSCRS::AgentClient::FieldGroup& tokenGroup);
    void setCertificates(const QList<LibreSCRS::AgentClient::CertificateInfo>& certs);
    void setCredentials(const LibreSCRS::AgentClient::CredentialList& credentials);

signals:
    void changePinRequested(const LibreSCRS::AgentClient::CredentialRecord& credential);
#ifdef LIBRECELIK_SIGNING_ENABLED
    void signRequested(const LibreSCRS::AgentClient::CertificateInfo& cert, const QString& cardId);
#endif
    void certificateDetailsRequested(const LibreSCRS::AgentClient::CertificateInfo& cert);

private slots:
    void onContextMenu(const QPoint& pos);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void applyTreeStyleSheet();
    void updateTreeMinimumHeight();
#ifdef LIBRECELIK_SIGNING_ENABLED
    QList<LibreSCRS::AgentClient::CertificateInfo> signingCertificates() const;
    void showCertificateDropdown();
#endif

    QList<LibreSCRS::AgentClient::CertificateInfo> certificateList;
    LibreSCRS::AgentClient::CredentialList credentialList;
    // Cached for retranslate-on-language-change: setTokenInfo() rebuilds
    // the header card from the original FieldGroup; the cache lets
    // retranslateUi re-run that path with the new translator active.
    LibreSCRS::AgentClient::FieldGroup tokenGroup;
    bool hasTokenGroup = false;
    QString cardId;

    QVBoxLayout* contentLayout = nullptr;
    librecelik::utils::CardHeaderCard* headerCard = nullptr;
    QTreeWidget* treeWidget = nullptr;
    QTreeWidgetItem* tokenCertsItem = nullptr;
    QTreeWidgetItem* tokenPinItem = nullptr;

#ifdef LIBRECELIK_SIGNING_ENABLED
    QToolButton* signBtn = nullptr;
#endif
};
