// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QWidget>
#include <QItemSelection>

typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;
typedef struct X509_name_st X509_NAME;

namespace Ui {
class CertificateViewerWidget;
}

class CertificateViewerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CertificateViewerWidget(X509* cert, X509_STORE* store, QWidget* parent = nullptr);
    ~CertificateViewerWidget();

private slots:
    void onDetailsSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onCertPathSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void populateGeneralTab(X509* cert, X509_STORE* store);

    static QString nameEntryValue(X509_NAME* name, int nid);
    static QString keyUsageString(X509* cert);

    Ui::CertificateViewerWidget* ui;
};
