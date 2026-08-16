// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "pdfpreviewwidget.h"

#include <QByteArray>
#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SignaturePlacementPage : public QWidget
{
    Q_OBJECT
public:
    explicit SignaturePlacementPage(QWidget* parent = nullptr);

    /// Hand the preview the layout source and the appearance font. Both are
    /// the signer's own, so the preview wraps and measures the way the stamped
    /// page will; the wizard owns the wiring because the widget dials nothing.
    void setLayoutProvider(PdfPreviewWidget::LayoutProvider provider);
    void setAppearanceFont(const QByteArray& ttfBytes);

    void loadPdf(const QString& path, const QString& signerName, const QString& issuer);
    bool isVisualSignatureEnabled() const;
    /// The wire's six-key visualSignature map for the current placement.
    /// Caller-guarded by @ref isVisualSignatureEnabled.
    [[nodiscard]] QVariantMap visualSignatureMap() const;
    void saveSettings() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void goToPreviousPage();
    void goToNextPage();
    void goToPage(int page);
    void updatePageLabel();

    void updatePreviewText();
    void retranslateUi();
    QString buildSignatureText() const;

    PdfPreviewWidget* preview = nullptr;
    QPushButton* prevBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QSpinBox* pageSpinBox = nullptr;
    QLabel* totalPagesLabel = nullptr;
    QCheckBox* visualSigCheckbox = nullptr;
    QLabel* reasonLabel = nullptr;
    QLineEdit* reasonEdit = nullptr;
    QLabel* locationLabel = nullptr;
    QLineEdit* locationEdit = nullptr;
    QString currentSignerName;
    QString currentIssuerName;
};
