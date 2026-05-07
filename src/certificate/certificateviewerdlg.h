// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDialog>

#include <LibreSCRS/Plugin/CardPlugin.h>

#include <memory>
#include <vector>

namespace LibreSCRS::Trust {
class TrustStore;
}

class QComboBox;
class QStackedWidget;

class CertificateViewerDlg : public QDialog
{
    Q_OBJECT
public:
    /// @brief Build a viewer dialog for the cards' certificates.
    /// @param certs        Per-card certificate list (DER + label).
    /// @param trustStore   Shared trust store used by the chain hierarchy
    ///                     tab. May be null — the dialog still opens, but
    ///                     the chain shows "trust unknown".
    /// @param parent       Optional Qt parent.
    /// @param initialIndex Which entry of @p certs to focus first.
    explicit CertificateViewerDlg(const std::vector<LibreSCRS::Plugin::CertificateData>& certs,
                                  std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore,
                                  QWidget* parent = nullptr, int initialIndex = 0);
    ~CertificateViewerDlg() override = default;

private:
    void buildUI(const std::vector<LibreSCRS::Plugin::CertificateData>& certs);

    std::shared_ptr<const LibreSCRS::Trust::TrustStore> trustStore;
    QComboBox* certCombo = nullptr;
    QStackedWidget* stack = nullptr;
};
