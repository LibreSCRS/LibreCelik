// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "config.h"

#include <LibreSCRS/Plugin/CardPlugin.h>

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"

#include <memory>
#include <string>
#include <vector>

namespace LibreSCRS::Trust {
class TrustStore;
}

class QEvent;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    /// @brief Build the token section.
    /// @param trustStore Shared trust store for certificate hierarchy / chain
    ///                   verification in the embedded viewer dialog. May be
    ///                   null in non-signing builds — the viewer degrades
    ///                   gracefully (per Spec §10).
    explicit TokenSection(std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore, QWidget* parent = nullptr);

#ifdef LIBRECELIK_SIGNING_ENABLED
    void setReaderName(const std::string& name);
#endif

public slots:
    void setTokenInfo(const LibreSCRS::Plugin::CardFieldGroup& tokenGroup);
    void setCertificates(const std::vector<LibreSCRS::Plugin::CertificateData>& certList);
    void setPINList(const std::vector<LibreSCRS::Plugin::PinStatusEntry>& pins);

signals:
    void changePINRequested(uint8_t pinReference, const QString& pinLabel, bool isTransport, int minLength,
                            int maxLength);
#ifdef LIBRECELIK_SIGNING_ENABLED
    void signRequested(const LibreSCRS::Plugin::CertificateData& cert, const std::string& readerName);
#endif

private slots:
    void onContextMenu(const QPoint& pos);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void applyTreeStyleSheet();
    void updateTreeMinimumHeight();
#ifdef LIBRECELIK_SIGNING_ENABLED
    std::vector<LibreSCRS::Plugin::CertificateData> signingCertificates() const;
    void showCertificateDropdown();
#endif

    std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore;
    std::vector<LibreSCRS::Plugin::CertificateData> certificateList;
    std::vector<LibreSCRS::Plugin::PinStatusEntry> pinList;
    // Cached for retranslate-on-language-change: setTokenInfo() rebuilds
    // the header card from the original CardFieldGroup; the cache lets
    // retranslateUi re-run that path with the new translator active.
    LibreSCRS::Plugin::CardFieldGroup tokenGroup;
    bool hasTokenGroup = false;

    QVBoxLayout* contentLayout = nullptr;
    LibreSCRS::CardHeaderCard* headerCard = nullptr;
    QTreeWidget* treeWidget = nullptr;
    QTreeWidgetItem* tokenCertsItem = nullptr;
    QTreeWidgetItem* tokenPinItem = nullptr;

#ifdef LIBRECELIK_SIGNING_ENABLED
    QToolButton* signBtn = nullptr;
    std::string readerName;
#endif
};
