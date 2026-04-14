// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "opensslhelpers.h"

#include <QDialog>
#include <plugin/card_plugin.h>

#include <vector>

class QComboBox;
class QStackedWidget;

class CertificateViewerDlg : public QDialog
{
    Q_OBJECT
public:
    explicit CertificateViewerDlg(const std::vector<plugin::CertificateData>& certs,
                                  const std::vector<std::string>& certPaths, QWidget* parent = nullptr,
                                  int initialIndex = 0);
    ~CertificateViewerDlg() = default;

private:
    void buildStore(const std::vector<std::string>& certPaths);
    void buildUI(const std::vector<plugin::CertificateData>& certs);

    struct ParsedCert
    {
        certutil::X509Ptr x509;
        QString label;
    };

    std::vector<ParsedCert> parsedCerts;
    certutil::X509StorePtr store;
    QComboBox* certCombo = nullptr;
    QStackedWidget* stack = nullptr;
};
