// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pdfpreviewwidget.h"

#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStringList>
#include <QtPdf/QPdfDocument>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr QColor kSigColor{45, 106, 79};
constexpr int kSigFillAlpha = 40;
constexpr int kSigBorderAlpha = 200;
constexpr qreal kSigBorderWidth = 2.0;
constexpr qreal kSigTextMargin = 4.0;
// Background color is derived from the widget palette at paint time (see paintEvent).
constexpr qreal kMinSigWidth = 80.0; // minimum signature rect size in PDF points
constexpr qreal kMinSigHeight = 30.0;
} // namespace

PdfPreviewWidget::PdfPreviewWidget(QWidget* parent) : QWidget(parent), document(std::make_unique<QPdfDocument>())
{
    setMouseTracking(true);
    setMinimumSize(200, 200);

    resizeTimer.setSingleShot(true);
    resizeTimer.setInterval(50);
    connect(&resizeTimer, &QTimer::timeout, this, [this]() {
        renderCurrentPage();
        update();
    });
}

PdfPreviewWidget::~PdfPreviewWidget() = default;

void PdfPreviewWidget::setLayoutProvider(LayoutProvider provider)
{
    layoutProvider = std::move(provider);
    cachedLayoutText.clear(); // invalidate cached layout (the answerer changed)
    update();
}

void PdfPreviewWidget::setAppearanceFont(const QByteArray& ttfBytes)
{
    if (ttfBytes == appearanceFontBytes)
        return;

    appearanceFontBytes = ttfBytes;
    appearanceFontFamily.clear();
    if (!ttfBytes.isEmpty()) {
        // addApplicationFontFromData returns -1 on unreadable bytes, and
        // applicationFontFamilies(-1) is empty — an unusable font degrades to
        // the family-by-name path rather than to a broken preview.
        const QStringList families =
            QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFontFromData(ttfBytes));
        if (!families.isEmpty())
            appearanceFontFamily = families.constFirst();
    }
    update();
}

bool PdfPreviewWidget::loadFile(const QString& path)
{
    auto err = document->load(path);
    if (err != QPdfDocument::Error::None)
        return false;

    // Default to last page (typical signing location)
    currentPageIndex = document->pageCount() - 1;

    // Place signature rect at bottom-right of the page
    const QSizeF pageSize = document->pagePointSize(currentPageIndex);
    constexpr qreal margin = 20.0;
    sigRect = QRectF(pageSize.width() - sigRect.width() - margin,
                     margin, // PDF y=0 is bottom, so small y = near bottom
                     sigRect.width(), sigRect.height());

    renderCurrentPage();
    update();
    return true;
}

int PdfPreviewWidget::pageCount() const
{
    return document->pageCount();
}

int PdfPreviewWidget::currentPage() const
{
    return currentPageIndex;
}

void PdfPreviewWidget::setCurrentPage(int page)
{
    if (page < 0 || page >= document->pageCount())
        return;
    currentPageIndex = page;
    renderCurrentPage();
    update();
}

QSizeF PdfPreviewWidget::pagePointSize() const
{
    if (document->pageCount() == 0)
        return {};
    return document->pagePointSize(currentPageIndex);
}

QRectF PdfPreviewWidget::signatureRect() const
{
    return sigRect;
}

void PdfPreviewWidget::setSignatureRect(const QRectF& rect)
{
    sigRect = rect;
    cachedLayoutText.clear(); // invalidate cached layout (rect changed)
    update();
}

void PdfPreviewWidget::setSignatureText(const QString& text)
{
    sigText = text;
    cachedLayoutText.clear(); // invalidate cached layout (text changed)
    update();
}

void PdfPreviewWidget::setSignatureVisible(bool visible)
{
    sigVisible = visible;
    update();
}

void PdfPreviewWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Fill background using the widget palette instead of a hardcoded color
    painter.fillRect(rect(), palette().color(QPalette::Window));

    // Draw rendered page
    if (!renderedPage.isNull()) {
        QRectF pageRect(offset, renderedPage.size());
        // Drop shadow
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 30));
        painter.drawRect(pageRect.adjusted(2, 2, 5, 5));
        // White page background (PDF may have transparent areas)
        painter.fillRect(pageRect, Qt::white);
        painter.drawImage(offset, renderedPage);
    }

    // Draw signature rectangle
    if (sigVisible && !renderedPage.isNull()) {
        const QRectF widgetSigRect = pdfRectToWidget(sigRect);

        // Semi-transparent fill
        QColor fill = kSigColor;
        fill.setAlpha(kSigFillAlpha);
        painter.fillRect(widgetSigRect, fill);

        // Dashed border
        QPen pen(kSigColor);
        pen.setWidthF(kSigBorderWidth);
        pen.setStyle(Qt::DashLine);
        pen.setColor(QColor(kSigColor.red(), kSigColor.green(), kSigColor.blue(), kSigBorderAlpha));
        painter.setPen(pen);
        painter.drawRect(widgetSigRect);

        // Signature text — FILL_BOX rendering driven by the signer's own
        // layout, fetched through the layout provider. The widget never
        // recomputes the wrap or font size locally; it just renders
        // `cachedLayout->lines` verbatim. Preview = PDF.
        if (!sigText.isEmpty()) {
            recomputeLayoutIfNeeded();

            // No layout to render means the placement box stands alone. The
            // preview does not fall back to a wrap of its own: a guessed
            // layout that disagrees with the stamp is worse than no preview
            // text at all, because it looks authoritative.
            if (cachedLayout) {
                painter.save();
                if (cachedLayout->clipped)
                    painter.setClipRect(widgetSigRect);

                painter.setPen(kSigColor);
                QFont f(appearanceFontFamily.isEmpty() ? QStringLiteral("Liberation Sans") : appearanceFontFamily);
                f.setPixelSize(std::max(1, static_cast<int>(std::lround(cachedLayout->fontSize * scale))));
                painter.setFont(f);

                const QRectF textRect = widgetSigRect.adjusted(kSigTextMargin * scale, kSigTextMargin * scale,
                                                               -kSigTextMargin * scale, -kSigTextMargin * scale);
                const qreal lineH = cachedLayout->lineHeight * scale;
                // Baseline placement mirrors the PAdES emitter's Tm exactly:
                //   the emitter places topBaselineY = height − margin − fontSize
                //   (PDF Y-up). In widget coords (Y-down mirror) that is
                //   textRect.top() + fontSize*scale. Using lineHeight here would
                //   shift the first line down by (leading − 1.0) × fontSize and
                //   break preview = PDF parity.
                qreal y = textRect.top() + cachedLayout->fontSize * scale;
                for (const auto& line : cachedLayout->lines) {
                    painter.drawText(QPointF(textRect.left(), y), line);
                    y += lineH;
                }
                painter.restore();
            }
        }
    }
}

PdfPreviewWidget::HitZone PdfPreviewWidget::hitTest(const QPointF& widgetPt) const
{
    const QRectF wr = pdfRectToWidget(sigRect);
    const qreal h = kHandleSize;

    // Corner handles (check first — they overlap edges)
    if (QRectF(wr.left() - h, wr.top() - h, 2 * h, 2 * h).contains(widgetPt))
        return HitZone::TopLeft;
    if (QRectF(wr.right() - h, wr.top() - h, 2 * h, 2 * h).contains(widgetPt))
        return HitZone::TopRight;
    if (QRectF(wr.left() - h, wr.bottom() - h, 2 * h, 2 * h).contains(widgetPt))
        return HitZone::BottomLeft;
    if (QRectF(wr.right() - h, wr.bottom() - h, 2 * h, 2 * h).contains(widgetPt))
        return HitZone::BottomRight;

    // Edge handles
    if (QRectF(wr.left() - h, wr.top(), 2 * h, wr.height()).contains(widgetPt))
        return HitZone::Left;
    if (QRectF(wr.right() - h, wr.top(), 2 * h, wr.height()).contains(widgetPt))
        return HitZone::Right;
    if (QRectF(wr.left(), wr.top() - h, wr.width(), 2 * h).contains(widgetPt))
        return HitZone::Top;
    if (QRectF(wr.left(), wr.bottom() - h, wr.width(), 2 * h).contains(widgetPt))
        return HitZone::Bottom;

    // Body
    if (wr.contains(widgetPt))
        return HitZone::Body;
    return HitZone::None;
}

Qt::CursorShape PdfPreviewWidget::cursorForZone(HitZone zone) const
{
    switch (zone) {
    case HitZone::TopLeft:
    case HitZone::BottomRight:
        return Qt::SizeFDiagCursor;
    case HitZone::TopRight:
    case HitZone::BottomLeft:
        return Qt::SizeBDiagCursor;
    case HitZone::Left:
    case HitZone::Right:
        return Qt::SizeHorCursor;
    case HitZone::Top:
    case HitZone::Bottom:
        return Qt::SizeVerCursor;
    case HitZone::Body:
        return Qt::OpenHandCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void PdfPreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (!sigVisible || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    activeZone = hitTest(event->position());
    if (activeZone == HitZone::Body) {
        const QRectF wr = pdfRectToWidget(sigRect);
        dragOffset = event->position() - wr.topLeft();
        setCursor(Qt::ClosedHandCursor);
    } else if (activeZone != HitZone::None) {
        setCursor(cursorForZone(activeZone));
    }
}

void PdfPreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!sigVisible) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QSizeF pageSize = document->pagePointSize(currentPageIndex);

    if (activeZone == HitZone::Body) {
        // Drag move
        QPointF newTopLeft = event->position() - dragOffset;
        QPointF pdfTopLeft = widgetToPdf(newTopLeft);
        qreal pdfX = pdfTopLeft.x();
        qreal pdfY = pdfTopLeft.y() - sigRect.height();
        pdfX = std::clamp(pdfX, 0.0, pageSize.width() - sigRect.width());
        pdfY = std::clamp(pdfY, 0.0, pageSize.height() - sigRect.height());
        sigRect.moveTopLeft({pdfX, pdfY});
        update();
    } else if (activeZone != HitZone::None) {
        // Resize
        QPointF pdfPt = widgetToPdf(event->position());
        qreal l = sigRect.left(), r = sigRect.right();
        qreal b = sigRect.top(), t = sigRect.bottom(); // PDF: top > bottom in y

        switch (activeZone) {
        case HitZone::Left:
            l = pdfPt.x();
            break;
        case HitZone::Right:
            r = pdfPt.x();
            break;
        case HitZone::Top:
            t = pdfPt.y();
            break;
        case HitZone::Bottom:
            b = pdfPt.y();
            break;
        case HitZone::TopLeft:
            l = pdfPt.x();
            t = pdfPt.y();
            break;
        case HitZone::TopRight:
            r = pdfPt.x();
            t = pdfPt.y();
            break;
        case HitZone::BottomLeft:
            l = pdfPt.x();
            b = pdfPt.y();
            break;
        case HitZone::BottomRight:
            r = pdfPt.x();
            b = pdfPt.y();
            break;
        default:
            break;
        }

        // Enforce minimum size and page bounds
        l = std::clamp(l, 0.0, pageSize.width() - kMinSigWidth);
        r = std::clamp(r, l + kMinSigWidth, pageSize.width());
        b = std::clamp(b, 0.0, pageSize.height() - kMinSigHeight);
        t = std::clamp(t, b + kMinSigHeight, pageSize.height());

        sigRect = QRectF(l, b, r - l, t - b);
        update();
    } else {
        // Hover cursor
        setCursor(cursorForZone(hitTest(event->position())));
    }
}

void PdfPreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (activeZone != HitZone::None && event->button() == Qt::LeftButton) {
        activeZone = HitZone::None;
        setCursor(cursorForZone(hitTest(event->position())));
    }
    QWidget::mouseReleaseEvent(event);
}

void PdfPreviewWidget::resizeEvent(QResizeEvent* /*event*/)
{
    resizeTimer.start();
}

// Convert a point from PDF coordinate space (origin bottom-left, units=points)
// to widget coordinate space (origin top-left, units=pixels).
//
// KNOWN LIMITATION — page rotation:
//   QPdfDocument in Qt 6.10 does not expose per-page /Rotate. We render
//   whatever pagePointSize() returns and trust the user-visible orientation
//   matches the underlying PDF user space. For pages with /Rotate set to
//   90/180/270, the placement preview can disagree with where libresign's
//   PAdES module ultimately stamps the signature, because the signing
//   module operates in unrotated PDF user space. TODO: parse /Rotate
//   from the page object directly, or extend libresign's
//   VisualSignatureParams to carry the rotation alongside the rect.
QPointF PdfPreviewWidget::pdfToWidget(const QPointF& pdfPt) const
{
    const QSizeF pageSize = document->pagePointSize(currentPageIndex);
    // Flip y: PDF y=0 is bottom, widget y=0 is top
    const qreal widgetX = pdfPt.x() * scale + offset.x();
    const qreal widgetY = (pageSize.height() - pdfPt.y()) * scale + offset.y();
    return {widgetX, widgetY};
}

// Convert a point from widget coordinate space to PDF coordinate space.
// See pdfToWidget for the rotation limitation.
QPointF PdfPreviewWidget::widgetToPdf(const QPointF& widgetPt) const
{
    const QSizeF pageSize = document->pagePointSize(currentPageIndex);
    const qreal pdfX = (widgetPt.x() - offset.x()) / scale;
    const qreal pdfY = pageSize.height() - (widgetPt.y() - offset.y()) / scale;
    return {pdfX, pdfY};
}

// Convert a PDF rect to widget rect.
// PDF rect: (x, y) is bottom-left corner, width/height extend right and up.
// Widget rect: (x, y) is top-left corner, width/height extend right and down.
QRectF PdfPreviewWidget::pdfRectToWidget(const QRectF& pdfRect) const
{
    // PDF rect top-left corner (in PDF space, y increases upward)
    const QPointF pdfTopLeft{pdfRect.x(), pdfRect.y() + pdfRect.height()};

    // In widget space, pdfTopLeft maps to the visual top-left
    const QPointF widgetTopLeft = pdfToWidget(pdfTopLeft);
    const qreal widgetW = pdfRect.width() * scale;
    const qreal widgetH = pdfRect.height() * scale;

    return {widgetTopLeft, QSizeF{widgetW, widgetH}};
}

void PdfPreviewWidget::recomputeLayoutIfNeeded() const
{
    const QSize boxSize = sigRect.size().toSize();
    if (cachedLayoutText == sigText && cachedLayoutBoxSize == boxSize)
        return;

    // The box goes over at the origin: the wrap depends on the box's size, not
    // on where the user dragged it, and the cache is keyed on the size alone.
    // Moving a box of unchanged size therefore costs no round trip.
    cachedLayout =
        layoutProvider ? layoutProvider(sigText, QRectF(0.0, 0.0, sigRect.width(), sigRect.height())) : std::nullopt;

    cachedLayoutText = sigText;
    cachedLayoutBoxSize = boxSize;
}

void PdfPreviewWidget::renderCurrentPage()
{
    if (document->pageCount() == 0) {
        renderedPage = {};
        return;
    }

    const QSizeF pageSize = document->pagePointSize(currentPageIndex);
    if (pageSize.isEmpty()) {
        renderedPage = {};
        return;
    }

    // Compute scale to fit page in widget while preserving aspect ratio
    const qreal scaleX = static_cast<qreal>(width()) / pageSize.width();
    const qreal scaleY = static_cast<qreal>(height()) / pageSize.height();
    scale = std::min(scaleX, scaleY);

    const QSize renderSize(static_cast<int>(pageSize.width() * scale), static_cast<int>(pageSize.height() * scale));

    // Center the page in the widget
    offset.setX((width() - renderSize.width()) / 2.0);
    offset.setY((height() - renderSize.height()) / 2.0);

    // Render via QPdfDocument
    renderedPage = document->render(currentPageIndex, renderSize);
}
