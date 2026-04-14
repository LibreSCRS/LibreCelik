// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "config.h"

#include <string>
#include <vector>
#include <plugin/card_plugin.h>
#include "utils/collapsiblesection.h"
#include "utils/cardheadercard.h"

class QEvent;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

class TokenSection : public CollapsibleSection
{
    Q_OBJECT
public:
    explicit TokenSection(std::vector<std::string> certPaths, QWidget* parent = nullptr);

#ifdef LIBRECELIK_SIGNING_ENABLED
    void setReaderName(const std::string& name);
#endif

public slots:
    void setTokenInfo(const plugin::CardFieldGroup& tokenGroup);
    void setCertificates(const std::vector<plugin::CertificateData>& certList);
    void setPINList(const std::vector<plugin::PinStatusEntry>& pins);

signals:
    void changePINRequested(uint8_t pinReference, const QString& pinLabel, bool isTransport, int minLength,
                            int maxLength);
#ifdef LIBRECELIK_SIGNING_ENABLED
    void signRequested(const plugin::CertificateData& cert, const std::string& readerName);
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
    std::vector<plugin::CertificateData> signingCertificates() const;
    void showCertificateDropdown();
#endif

    std::vector<std::string> certPaths;
    std::vector<plugin::CertificateData> certificateList;
    std::vector<plugin::PinStatusEntry> pinList;

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
