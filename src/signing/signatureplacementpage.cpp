// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signatureplacementpage.h"

#include "pdfpreviewwidget.h"
#include "settings/settingskeys.h"

#include <QCheckBox>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
constexpr int kNavButtonWidth = 32;
constexpr int kPageSpinBoxWidth = 60;
} // namespace

SignaturePlacementPage::SignaturePlacementPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    // PDF preview
    preview = new PdfPreviewWidget(this);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(preview, 1);

    // Bottom bar
    auto* bottomBar = new QHBoxLayout;

    // Page navigation group: [<] [SpinBox] of N [>]  — kept compact
    auto* navGroup = new QHBoxLayout;
    navGroup->setSpacing(2);

    prevBtn = new QPushButton(QStringLiteral("<"), this);
    prevBtn->setFixedWidth(kNavButtonWidth);
    navGroup->addWidget(prevBtn);

    pageSpinBox = new QSpinBox(this);
    pageSpinBox->setMinimum(1);
    pageSpinBox->setMaximum(1);
    pageSpinBox->setAlignment(Qt::AlignCenter);
    pageSpinBox->setFixedWidth(kPageSpinBoxWidth);
    navGroup->addWidget(pageSpinBox);

    totalPagesLabel = new QLabel(this);
    totalPagesLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    navGroup->addWidget(totalPagesLabel);

    nextBtn = new QPushButton(QStringLiteral(">"), this);
    nextBtn->setFixedWidth(kNavButtonWidth);
    navGroup->addWidget(nextBtn);

    bottomBar->addLayout(navGroup);
    bottomBar->addStretch();

    visualSigCheckbox = new QCheckBox(this);
    visualSigCheckbox->setChecked(true);
    bottomBar->addWidget(visualSigCheckbox);

    layout->addLayout(bottomBar);

    // Reason / Location fields (optional, shown when visual sig is enabled)
    auto* fieldsWidget = new QWidget(this);
    auto* fieldsRow = new QHBoxLayout(fieldsWidget);
    fieldsRow->setContentsMargins(0, 0, 0, 0);
    reasonLabel = new QLabel(fieldsWidget);
    fieldsRow->addWidget(reasonLabel);
    reasonEdit = new QLineEdit(fieldsWidget);
    fieldsRow->addWidget(reasonEdit, 1);
    fieldsRow->addSpacing(10);
    locationLabel = new QLabel(fieldsWidget);
    fieldsRow->addWidget(locationLabel);
    locationEdit = new QLineEdit(fieldsWidget);
    fieldsRow->addWidget(locationEdit, 1);
    layout->addWidget(fieldsWidget);

    // Connections
    connect(prevBtn, &QPushButton::clicked, this, &SignaturePlacementPage::goToPreviousPage);
    connect(nextBtn, &QPushButton::clicked, this, &SignaturePlacementPage::goToNextPage);
    connect(pageSpinBox, &QSpinBox::valueChanged, this, &SignaturePlacementPage::goToPage);
    connect(visualSigCheckbox, &QCheckBox::toggled, preview, &PdfPreviewWidget::setSignatureVisible);
    connect(visualSigCheckbox, &QCheckBox::toggled, fieldsWidget, &QWidget::setVisible);
    connect(reasonEdit, &QLineEdit::textChanged, this, &SignaturePlacementPage::updatePreviewText);
    connect(locationEdit, &QLineEdit::textChanged, this, &SignaturePlacementPage::updatePreviewText);

    // Restore persisted reason/location
    QSettings settings(settings::kOrganization, settings::kApplication);
    reasonEdit->setText(settings.value(settings::kSigningReason).toString());
    locationEdit->setText(settings.value(settings::kSigningLocation).toString());

    // Apply translations once at end of construction; LanguageChange
    // re-runs retranslateUi() (single source of truth).
    retranslateUi();
}

void SignaturePlacementPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void SignaturePlacementPage::retranslateUi()
{
    prevBtn->setToolTip(qtTrId("lc-sign-page-prev"));
    nextBtn->setToolTip(qtTrId("lc-sign-page-next"));
    visualSigCheckbox->setText(qtTrId("lc-sign-visual-sig"));
    reasonLabel->setText(qtTrId("lc-sign-visual-reason"));
    reasonEdit->setPlaceholderText(qtTrId("lc-sign-visual-reason-placeholder"));
    locationLabel->setText(qtTrId("lc-sign-visual-location"));
    locationEdit->setPlaceholderText(qtTrId("lc-sign-visual-location-placeholder"));
    updatePageLabel();
    updatePreviewText();
}

void SignaturePlacementPage::loadPdf(const QString& path, const QString& signerName, const QString& issuer)
{
    currentSignerName = signerName;
    currentIssuerName = issuer;
    preview->loadFile(path);
    updatePreviewText();
    updatePageLabel();
}

QString SignaturePlacementPage::buildSignatureText() const
{
    QString text = qtTrId("lc-sign-visual-text-signed-by") + QStringLiteral(" ") + currentSignerName;
    if (!currentIssuerName.isEmpty())
        text +=
            QStringLiteral("\n") + qtTrId("lc-sign-visual-text-issued-by") + QStringLiteral(" ") + currentIssuerName;
    text += QStringLiteral("\n") + qtTrId("lc-sign-visual-text-date") + QStringLiteral(" ") +
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!reasonEdit->text().trimmed().isEmpty())
        text += QStringLiteral("\n") + qtTrId("lc-sign-visual-text-reason") + QStringLiteral(" ") +
                reasonEdit->text().trimmed();
    if (!locationEdit->text().trimmed().isEmpty())
        text += QStringLiteral("\n") + qtTrId("lc-sign-visual-text-location") + QStringLiteral(" ") +
                locationEdit->text().trimmed();
    return text;
}

void SignaturePlacementPage::updatePreviewText()
{
    preview->setSignatureText(buildSignatureText());
}

bool SignaturePlacementPage::isVisualSignatureEnabled() const
{
    return visualSigCheckbox->isChecked();
}

LibreSCRS::Signing::VisualSignatureParams SignaturePlacementPage::visualParams() const
{
    // Caller guards this with isVisualSignatureEnabled(); the placement page
    // is only asked for params when the checkbox is on, and the wizard
    // passes std::nullopt otherwise. Builder validators (pageIndex >= 0,
    // positive width/height) would reject an empty call anyway.
    const QRectF rect = preview->signatureRect();

    // signatureRect() is in PDF user space (origin bottom-left). The native
    // PAdES engine writes these values straight into /Rect which is also in
    // PDF user space, so pass through unchanged. A historical y-flip here
    // converted to DSS's top-left origin and caused a double-transform once
    // the DSS backend was retired (main commit 1185e24 fixed the regression).
    const int x = static_cast<int>(rect.x());
    const int y = static_cast<int>(rect.y());
    const int w = static_cast<int>(rect.width());
    const int h = static_cast<int>(rect.height());

    LibreSCRS::Signing::VisualSignatureParams::Builder b;
    b.pageIndex(preview->currentPage())
        .rect(LibreSCRS::Signing::Rect{x, y, w, h})
        .textTemplate(buildSignatureText().toStdString());
    return std::move(b).build();
}

void SignaturePlacementPage::saveSettings() const
{
    QSettings settings(settings::kOrganization, settings::kApplication);
    settings.setValue(settings::kSigningReason, reasonEdit->text());
    settings.setValue(settings::kSigningLocation, locationEdit->text());
}

void SignaturePlacementPage::goToPreviousPage()
{
    const int page = preview->currentPage();
    if (page > 0) {
        preview->setCurrentPage(page - 1);
        updatePageLabel();
    }
}

void SignaturePlacementPage::goToNextPage()
{
    const int page = preview->currentPage();
    if (page < preview->pageCount() - 1) {
        preview->setCurrentPage(page + 1);
        updatePageLabel();
    }
}

void SignaturePlacementPage::goToPage(int page)
{
    // page is 1-based from the spinbox, preview uses 0-based
    const int zeroBasedPage = page - 1;
    if (zeroBasedPage >= 0 && zeroBasedPage < preview->pageCount() && zeroBasedPage != preview->currentPage()) {
        preview->setCurrentPage(zeroBasedPage);
        updatePageLabel();
    }
}

void SignaturePlacementPage::updatePageLabel()
{
    const int current = preview->currentPage() + 1;
    const int total = preview->pageCount();

    // Update spinbox range and value without triggering goToPage recursion
    pageSpinBox->blockSignals(true);
    pageSpinBox->setMaximum(total > 0 ? total : 1);
    pageSpinBox->setValue(current);
    pageSpinBox->blockSignals(false);

    //% "of %1"
    totalPagesLabel->setText(qtTrId("lc-sign-page-of-total").arg(total));

    prevBtn->setEnabled(current > 1);
    nextBtn->setEnabled(current < total);
}
