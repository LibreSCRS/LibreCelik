// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <memory>
#include <optional>

class QPdfDocument;

class PdfPreviewWidget : public QWidget
{
    Q_OBJECT
public:
    /// Supplies the visual-signature layout for a (text, box) pair, the box in
    /// PDF user units — the same question the signer asks before it stamps, so
    /// that preview and stamp agree on the wrap and the font size. A `nullopt`
    /// answer means no layout is available (no provider wired yet, or an agent
    /// without the layout-preview feature); the preview then draws the bare
    /// placement box rather than inventing a wrap of its own.
    using LayoutProvider =
        std::function<std::optional<LibreSCRS::AgentClient::LayoutResult>(const QString& text, QRectF boxPdfUnits)>;

    explicit PdfPreviewWidget(QWidget* parent = nullptr);
    ~PdfPreviewWidget() override;

    void setLayoutProvider(LayoutProvider provider);
    /// Installs the appearance font the signer embeds, so the preview measures
    /// glyphs the way the stamped signature does. Registration happens once
    /// per distinct byte string; empty or unreadable bytes leave the preview
    /// asking for the font family by name.
    void setAppearanceFont(const QByteArray& ttfBytes);

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
    // Refresh `cachedLayout` from `layoutProvider` when (sigText,
    // sigRect.size()) has changed. Call from paintEvent before reading
    // `cachedLayout`. `mutable` allows use from `const` paint context
    // (paint-time caching, no observable side effect).
    void recomputeLayoutIfNeeded() const;

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
    LayoutProvider layoutProvider;
    // Bytes last handed to `setAppearanceFont`, and the family they resolved
    // to. The bytes are kept so the same font is registered once, not once per
    // call; an empty family means "ask for the default family by name".
    QByteArray appearanceFontBytes;
    QString appearanceFontFamily;
    // Cached provider answer, keyed by (cachedLayoutText, cachedLayoutBoxSize).
    // Mutated from `paintEvent` via `recomputeLayoutIfNeeded()`; invalidated
    // by `setLayoutProvider`, `setSignatureRect` and `setSignatureText` (which
    // clear `cachedLayoutText`).
    mutable std::optional<LibreSCRS::AgentClient::LayoutResult> cachedLayout;
    mutable QString cachedLayoutText;
    mutable QSize cachedLayoutBoxSize;
    HitZone activeZone = HitZone::None;
    QPointF dragOffset;
    QTimer resizeTimer;
    static constexpr qreal kHandleSize = 6.0;
};
