// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QItemSelection>
#include <QWidget>

#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <vector>

namespace LibreSCRS::Trust {
class TrustStore;
}

namespace Ui {
class CertificateViewerWidget;
}

class CertificateViewerWidget : public QWidget
{
    Q_OBJECT
public:
    /// @brief Build a viewer for a single DER-encoded leaf certificate.
    /// @param der   Owning copy of the certificate DER. The widget parses it once
    ///              and feeds the resulting ParsedCertificate to both the details
    ///              and the chain model. Stored as `leafCertDer`.
    /// @param store Shared trust store used for chain hierarchy / status display.
    ///              May be null — the chain tab degrades to "trust unknown".
    explicit CertificateViewerWidget(std::vector<std::uint8_t> der,
                                     std::shared_ptr<const LibreSCRS::Trust::TrustStore> store,
                                     QWidget* parent = nullptr);
    ~CertificateViewerWidget() override;

private slots:
    void onDetailsSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onCertPathSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void populateGeneralTab();
    void populateUnparseableTab();

    Ui::CertificateViewerWidget* ui;
    std::vector<std::uint8_t> leafCertDer;
    std::expected<LibreSCRS::Certificate::ParsedCertificate, LibreSCRS::Certificate::ParsedCertificate::ParseError>
        parsedCert;
    std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore;
};
