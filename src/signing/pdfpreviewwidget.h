// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QTimer>
#include <QWidget>

#include <memory>

class QPdfDocument;

class PdfPreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PdfPreviewWidget(QWidget* parent = nullptr);
    ~PdfPreviewWidget() override;

    bool loadFile(const QString& path);
    int pageCount() const;
    int currentPage() const;
    void setCurrentPage(int page);

    // Current page size in PDF points (72 dpi)
    QSizeF pagePointSize() const;

    // Signature rectangle in PDF points (72 dpi)
    QRectF signatureRect() const;
    void setSignatureRect(const QRectF& rect);
    void setSignatureText(const QString& text);
    void setSignatureVisible(bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointF pdfToWidget(const QPointF& pdfPt) const;
    QPointF widgetToPdf(const QPointF& widgetPt) const;
    QRectF pdfRectToWidget(const QRectF& pdfRect) const;
    void renderCurrentPage();
    qreal computeFillBoxFontSize(const QRectF& pdfRect, const QString& text);

    std::unique_ptr<QPdfDocument> document;
    QImage renderedPage;
    int currentPageIndex = 0;
    qreal scale = 1.0;
    QPointF offset;

    enum class HitZone { None, Body, TopLeft, TopRight, BottomLeft, BottomRight, Left, Right, Top, Bottom };
    HitZone hitTest(const QPointF& widgetPt) const;
    Qt::CursorShape cursorForZone(HitZone zone) const;

    QRectF sigRect{0, 0, 200, 50};
    QString sigText;
    bool sigVisible = true;
    qreal cachedFontSize = -1;
    QSizeF cachedSigSize;
    QString cachedSigText;
    HitZone activeZone = HitZone::None;
    QPointF dragOffset;
    QTimer resizeTimer;
    static constexpr qreal kHandleSize = 6.0;
};
