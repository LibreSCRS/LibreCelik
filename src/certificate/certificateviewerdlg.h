// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QDialog>
#include <plugin/card_plugin.h>

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <vector>

class QComboBox;
class QStackedWidget;

class CertificateViewerDlg : public QDialog
{
    Q_OBJECT
public:
    explicit CertificateViewerDlg(const std::vector<plugin::CertificateData>& certs, const std::string& certFolderPath,
                                  QWidget* parent = nullptr, int initialIndex = 0);
    ~CertificateViewerDlg();

private:
    void buildStore(const std::string& certFolderPath);
    void buildUI(const std::vector<plugin::CertificateData>& certs);

    struct ParsedCert
    {
        X509* x509 = nullptr;
        QString label;
    };

    std::vector<ParsedCert> parsedCerts;
    X509_STORE* store = nullptr;
    QComboBox* certCombo = nullptr;
    QStackedWidget* stack = nullptr;
};
