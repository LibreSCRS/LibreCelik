// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerdlg.h"
#include "certificateviewerwidget.h"

#include "utils/buttonbox.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

CertificateViewerDlg::CertificateViewerDlg(const std::vector<LibreSCRS::Plugin::CertificateData>& certs,
                                           std::shared_ptr<const LibreSCRS::Trust::TrustStore> store, QWidget* parent,
                                           int initialIndex)
    : QDialog(parent), trustStore(std::move(store))
{
    setWindowTitle(
        qtTrId("lc-cert-dialog-title")); // i18n-audit: ignore D1, modal dialog re-opened fresh after language switch
    resize(600, 500);

    buildUI(certs);

    if (initialIndex > 0 && stack && certCombo) {
        certCombo->setCurrentIndex(initialIndex);
    } else if (initialIndex > 0 && stack) {
        stack->setCurrentIndex(initialIndex);
    }
}

void CertificateViewerDlg::buildUI(const std::vector<LibreSCRS::Plugin::CertificateData>& certs)
{
    auto* layout = new QVBoxLayout(this);

    if (certs.empty()) {
        layout->addWidget(new QLabel(qtTrId("lc-cert-no-available")));
        auto* buttons = new librecelik::ButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
        return;
    }

    if (certs.size() > 1) {
        certCombo = new QComboBox(this);
        for (const auto& cd : certs)
            certCombo->addItem(QString::fromStdString(cd.label));
        layout->addWidget(certCombo);
    }

    stack = new QStackedWidget(this);
    for (const auto& cd : certs) {
        auto* viewer = new CertificateViewerWidget(cd.derBytes, trustStore, this);
        stack->addWidget(viewer);
    }
    layout->addWidget(stack);

    if (certCombo)
        connect(certCombo, &QComboBox::currentIndexChanged, stack, &QStackedWidget::setCurrentIndex);

    auto* buttons = new librecelik::ButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}
