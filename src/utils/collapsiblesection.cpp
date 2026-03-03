// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "collapsiblesection.h"

#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>

static const QColor HEADER_BG  { 61, 140, 149 };   // teal
static const QColor FRAME_BORDER{ 61, 140, 149, 80 };

CollapsibleSection::CollapsibleSection(QWidget* parent)
    : QGroupBox(parent)
{
    init();
}

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent)
    : QGroupBox(title, parent)
{
    init();
}

void CollapsibleSection::init()
{
    setCheckable(false);
    setMinimumHeight(0);
    setContentsMargins(2, HEADER_HEIGHT + 4, 2, 4);

    animation = new QPropertyAnimation(this, "sectionHeight", this);
    animation->setDuration(200);
    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        if (!expanded) {
            setChildrenVisible(false);
        } else {
            setMaximumHeight(QWIDGETSIZE_MAX);
        }
    });
}

void CollapsibleSection::setChildrenVisible(bool visible)
{
    auto children = findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (auto* child : std::as_const(children))
        child->setVisible(visible);
}

void CollapsibleSection::applyCollapsed()
{
    expandedHeight = sizeHint().height();
    setChildrenVisible(false);
    setMaximumHeight(HEADER_HEIGHT);
}

void CollapsibleSection::setExpanded(bool exp)
{
    if (expanded == exp)
        return;
    expanded = exp;
    update();

    if (!isVisible()) {
        if (!expanded)
            applyCollapsed();
        else {
            setChildrenVisible(true);
            setMaximumHeight(QWIDGETSIZE_MAX);
        }
        return;
    }

    if (expanded) {
        setChildrenVisible(true);
        int target = (expandedHeight > HEADER_HEIGHT) ? expandedHeight : sizeHint().height();
        animation->setStartValue(HEADER_HEIGHT);
        animation->setEndValue(target);
    } else {
        expandedHeight = height();
        animation->setStartValue(height());
        animation->setEndValue(HEADER_HEIGHT);
    }
    animation->start();
}

void CollapsibleSection::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Header background
    p.fillRect(0, 0, width(), HEADER_HEIGHT, HEADER_BG);

    // Arrow glyph
    p.setPen(Qt::white);
    QFont af = font();
    af.setPointSizeF(af.pointSizeF() * 0.85);
    p.setFont(af);
    p.drawText(QRect(8, 0, 18, HEADER_HEIGHT), Qt::AlignVCenter | Qt::AlignHCenter,
               expanded ? QStringLiteral("▼") : QStringLiteral("▶"));

    // Title
    QFont tf = font();
    tf.setBold(true);
    p.setFont(tf);
    p.drawText(QRect(30, 0, width() - 34, HEADER_HEIGHT),
               Qt::AlignVCenter | Qt::AlignLeft, title());

    // Subtle border around content area
    p.setPen(QPen(FRAME_BORDER, 1));
    p.drawRect(0, HEADER_HEIGHT, width() - 1, height() - HEADER_HEIGHT - 1);
}

void CollapsibleSection::mousePressEvent(QMouseEvent* event)
{
    if (event->position().y() <= HEADER_HEIGHT)
        setExpanded(!expanded);
    else
        QGroupBox::mousePressEvent(event);
}

// QGroupBox::setTitle() and QGroupBox::changeEvent() both call calculateFrame()
// internally, which resets our content margins to style-computed values.  On
// macOS the Aqua style computes a smaller top margin than HEADER_HEIGHT, causing
// the content widgets to overlap the painted header.  Restore the correct margins
// after every such reset.

void CollapsibleSection::setTitle(const QString& title)
{
    QGroupBox::setTitle(title);
    setContentsMargins(2, HEADER_HEIGHT + 4, 2, 4);
}

void CollapsibleSection::changeEvent(QEvent* event)
{
    QGroupBox::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        setContentsMargins(2, HEADER_HEIGHT + 4, 2, 4);
}

void CollapsibleSection::showEvent(QShowEvent* event)
{
    // Ensure margins are correct at first show (calculateFrame may have run
    // between init() and here via setTitle called from .ui setup).
    setContentsMargins(2, HEADER_HEIGHT + 4, 2, 4);
    QGroupBox::showEvent(event);
}
