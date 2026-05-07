// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Signing/VisualSignatureParams.h>

#include <QWidget>

class QCheckBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class PdfPreviewWidget;

class SignaturePlacementPage : public QWidget
{
    Q_OBJECT
public:
    explicit SignaturePlacementPage(QWidget* parent = nullptr);

    void loadPdf(const QString& path, const QString& signerName, const QString& issuer);
    bool isVisualSignatureEnabled() const;
    LibreSCRS::Signing::VisualSignatureParams visualParams() const;
    void saveSettings() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void goToPreviousPage();
    void goToNextPage();
    void goToPage(int page);
    void updatePageLabel();

    void updatePreviewText();
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
