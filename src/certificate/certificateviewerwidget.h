// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CERTIFICATEVIEWERWIDGET_H
#define CERTIFICATEVIEWERWIDGET_H

#include <QWidget>
#include <QItemSelection>

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

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
    static QString asnTimeToString(const ASN1_TIME* time);
    static QString keyUsageString(X509* cert);

    Ui::CertificateViewerWidget* ui;
};

#endif // CERTIFICATEVIEWERWIDGET_H
