// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "collapsiblesection.h"

#include <QMouseEvent>
#include <QPainter>

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
    setContentsMargins(2, HEADER_H + 4, 2, 4);

    m_animation = new QPropertyAnimation(this, "sectionHeight", this);
    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (!m_expanded) {
            setChildrenVisible(false);
        } else {
            setMaximumHeight(QWIDGETSIZE_MAX);
        }
    });
}

void CollapsibleSection::setChildrenVisible(bool visible)
{
    for (auto* child : findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
        child->setVisible(visible);
}

void CollapsibleSection::applyCollapsed()
{
    m_expandedHeight = sizeHint().height();
    setChildrenVisible(false);
    setMaximumHeight(HEADER_H);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
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
        int target = (m_expandedHeight > HEADER_H) ? m_expandedHeight : sizeHint().height();
        m_animation->setStartValue(HEADER_H);
        m_animation->setEndValue(target);
    } else {
        m_expandedHeight = height();
        m_animation->setStartValue(height());
        m_animation->setEndValue(HEADER_H);
    }
    m_animation->start();
}

void CollapsibleSection::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Header background
    p.fillRect(0, 0, width(), HEADER_H, HEADER_BG);

    // Arrow glyph
    p.setPen(Qt::white);
    QFont af = font();
    af.setPointSizeF(af.pointSizeF() * 0.85);
    p.setFont(af);
    p.drawText(QRect(8, 0, 18, HEADER_H), Qt::AlignVCenter | Qt::AlignHCenter,
               m_expanded ? QStringLiteral("▼") : QStringLiteral("▶"));

    // Title
    QFont tf = font();
    tf.setBold(true);
    p.setFont(tf);
    p.drawText(QRect(30, 0, width() - 34, HEADER_H),
               Qt::AlignVCenter | Qt::AlignLeft, title());

    // Subtle border around content area
    p.setPen(QPen(FRAME_BORDER, 1));
    p.drawRect(0, HEADER_H, width() - 1, height() - HEADER_H - 1);
}

void CollapsibleSection::mousePressEvent(QMouseEvent* event)
{
    if (event->position().y() <= HEADER_H)
        setExpanded(!m_expanded);
    else
        QGroupBox::mousePressEvent(event);
}
