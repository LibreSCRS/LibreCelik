// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "filedropzone.h"

#include "signingcolors.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>

FileDropZone::FileDropZone(QWidget* parent) : QWidget(parent)
{
    setAcceptDrops(true);
    setMinimumHeight(100);
    setCursor(Qt::PointingHandCursor);
}

QStringList FileDropZone::filePaths() const
{
    return files;
}

void FileDropZone::clear()
{
    files.clear();
    emit filesChanged(files);
}

void FileDropZone::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::LanguageChange)
        update();
    QWidget::changeEvent(event);
}

void FileDropZone::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        dragOver = true;
        update();
    }
}

void FileDropZone::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    dragOver = false;
    update();
}

void FileDropZone::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }

    if (!paths.isEmpty()) {
        addFiles(paths);
    }

    dragOver = false;
    update();
}

void FileDropZone::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    browseFiles();
}

void FileDropZone::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor teal = signing::kTealColor;
    const int radius = 12;
    const int margin = 1;
    const QRectF area = QRectF(rect()).adjusted(margin, margin, -margin, -margin);

    // Background fill
    QColor bgColor = teal;
    bgColor.setAlpha(dragOver ? 25 : 12);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(area, radius, radius);

    // Border
    QPen borderPen;
    borderPen.setColor(teal);
    borderPen.setWidth(2);
    borderPen.setStyle(dragOver ? Qt::SolidLine : Qt::DashLine);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(area, radius, radius);

    // Icon circle (48px) centered horizontally, positioned above text
    const qreal centerX = area.center().x();
    const qreal topY = area.top() + 20;
    const QRectF circleRect(centerX - 24, topY, 48, 48);

    QColor circleColor = teal;
    circleColor.setAlpha(38);
    painter.setPen(Qt::NoPen);
    painter.setBrush(circleColor);
    painter.drawEllipse(circleRect);

    // Document icon inside circle
    QPen iconPen(teal, 1.5);
    painter.setPen(iconPen);
    painter.setBrush(Qt::NoBrush);

    const qreal cx = circleRect.center().x();
    const qreal cy = circleRect.center().y();
    const qreal docW = 14;
    const qreal docH = 18;
    const qreal fold = 5;

    QPolygonF docShape;
    docShape << QPointF(cx - docW / 2, cy - docH / 2) << QPointF(cx + docW / 2 - fold, cy - docH / 2)
             << QPointF(cx + docW / 2, cy - docH / 2 + fold) << QPointF(cx + docW / 2, cy + docH / 2)
             << QPointF(cx - docW / 2, cy + docH / 2);
    docShape << docShape.first();
    painter.drawPolyline(docShape);

    // Fold line
    painter.drawLine(QPointF(cx + docW / 2 - fold, cy - docH / 2), QPointF(cx + docW / 2 - fold, cy - docH / 2 + fold));
    painter.drawLine(QPointF(cx + docW / 2 - fold, cy - docH / 2 + fold), QPointF(cx + docW / 2, cy - docH / 2 + fold));

    // Primary text
    QFont primaryFont = font();
    primaryFont.setPointSize(10);
    primaryFont.setWeight(QFont::Medium);
    painter.setFont(primaryFont);
    painter.setPen(palette().color(QPalette::Text));

    const qreal textY = circleRect.bottom() + 12;
    //% "Drop PDF or other files here"
    const QString primaryText = qtTrId("lc-sign-drop-primary");
    QRectF primaryRect(area.left(), textY, area.width(), 20);
    painter.drawText(primaryRect, Qt::AlignHCenter | Qt::AlignTop, primaryText);

    // Secondary text: "or" + "browse files"
    QFont secondaryFont = font();
    secondaryFont.setPointSize(10);
    painter.setFont(secondaryFont);

    //% "or"
    const QString orText = qtTrId("lc-sign-drop-or");
    //% "browse files"
    const QString browseText = qtTrId("lc-sign-drop-browse");

    const QFontMetrics fm(secondaryFont);
    const int orWidth = fm.horizontalAdvance(orText);
    const int browseWidth = fm.horizontalAdvance(browseText);
    const int spacing = fm.horizontalAdvance(QStringLiteral(" "));
    const int totalWidth = orWidth + spacing + browseWidth;
    const qreal secondaryY = textY + 22;
    const qreal startX = centerX - totalWidth / 2.0;

    // "or" in PlaceholderText color
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(QPointF(startX, secondaryY + fm.ascent()), orText);

    // "browse files" in teal underlined
    QFont browseFont = secondaryFont;
    browseFont.setUnderline(true);
    painter.setFont(browseFont);
    painter.setPen(teal);
    painter.drawText(QPointF(startX + orWidth + spacing, secondaryY + fm.ascent()), browseText);
}

void FileDropZone::removeFile(const QString& path)
{
    files.removeAll(path);
}

void FileDropZone::addFiles(const QStringList& paths)
{
    // Centralized validation: drag-and-drop accepts arbitrary local file
    // URLs (folders, symlinks, sockets, deleted-but-still-in-history paths,
    // special files, etc.). Filter to regular files we can actually read
    // before they reach the signing pipeline. The QFileDialog browse path
    // funnels through here too, so the same checks apply to both.
    bool added = false;
    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.exists() || !info.isFile() || !info.isReadable())
            continue;
        if (!files.contains(path)) {
            files.append(path);
            added = true;
        }
    }
    if (added)
        emit filesChanged(files);
}

void FileDropZone::browseFiles()
{
    //% "Select files"
    const QStringList paths = QFileDialog::getOpenFileNames(this, qtTrId("lc-sign-select-files"));
    if (!paths.isEmpty()) {
        addFiles(paths);
    }
}
