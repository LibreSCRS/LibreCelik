// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "collapsiblesection.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QToolButton>
#include <QShowEvent>

CollapsibleSection::CollapsibleSection(QWidget* parent) : QGroupBox(parent)
{
    init();
}

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent) : QGroupBox(title, parent)
{
    setAccessibleName(title);
    init();
}

CollapsibleSection::CollapsibleSection(const QString& title, const QColor& headerColor, QWidget* parent)
    : QGroupBox(title, parent)
{
    setAccessibleName(title);
    init();
    setHeaderColor(headerColor);
}

QColor CollapsibleSection::headerColor() const
{
    return headerBg;
}

void CollapsibleSection::setHeaderColor(const QColor& color)
{
    headerBg = color;
    frameBorder = QColor(color.red(), color.green(), color.blue(), 80);
    update();
}

void CollapsibleSection::setHeaderHeight(int h)
{
    headerHeight = h;
    setContentsMargins(2, h + 4, 2, 4);
    if (!expanded)
        setMaximumHeight(h);
    repositionHeaderWidgets();
    update();
}

void CollapsibleSection::init()
{
    setCheckable(false);
    setMinimumHeight(0);
    setFocusPolicy(Qt::TabFocus);
    setContentsMargins(2, headerHeight + 4, 2, 4);

    animation = new QPropertyAnimation(this, "sectionHeight", this);
    animation->setDuration(200);
    animation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(animation, &QPropertyAnimation::finished, this, [this]() {
        if (!expanded) {
            setChildrenVisible(false);
        } else {
            setMaximumHeight(QWIDGETSIZE_MAX);
            emit sectionExpanded();
        }
        updateGeometry();
    });
}

void CollapsibleSection::addHeaderWidget(QWidget* w)
{
    w->setParent(this);
    w->setFixedHeight(22);
    if (auto* btn = qobject_cast<QPushButton*>(w)) {
        auto bg = headerBg.lighter(115);
        auto border = headerBg.lighter(130);
        auto hover = headerBg.lighter(120);
        auto pressed = headerBg.darker(115);
        auto disabledText = headerBg.lighter(180);

        auto style = QString(R"(
            QPushButton {
                color: white; background: %1; border: 1px solid %2;
                border-radius: 3px; padding: 2px 8px; font-size: 11px;
            }
            QPushButton:hover { background: %3; }
            QPushButton:pressed { background: %4; }
            QPushButton:disabled { color: %5; background: %6; }
        )")
                         .arg(bg.name(), border.name(), hover.name(), pressed.name(), disabledText.name(),
                              headerBg.darker(110).name());
        btn->setStyleSheet(style);
    } else if (auto* toolBtn = qobject_cast<QToolButton*>(w)) {
        toolBtn->setAutoRaise(true);
        toolBtn->setStyleSheet(
            QStringLiteral("QToolButton { background: transparent; border: none; }"
                           "QToolButton:hover { background: rgba(255,255,255,40); border-radius: 3px; }"
                           "QToolButton:pressed { background: rgba(0,0,0,30); border-radius: 3px; }"));
    }
    headerWidgets.append(w);
    repositionHeaderWidgets();
    w->show();
}

void CollapsibleSection::repositionHeaderWidgets()
{
    if (headerWidgets.isEmpty())
        return;
    int x = width() - 4;
    for (int i = headerWidgets.size() - 1; i >= 0; --i) {
        QWidget* w = headerWidgets[i];
        int ww = w->sizeHint().width();
        if (ww < 1)
            ww = w->width();
        x -= ww + 4;
        w->setGeometry(x, (headerHeight - w->height()) / 2, ww, w->height());
        w->raise();
    }
    update();
}

void CollapsibleSection::setChildrenVisible(bool visible)
{
    auto children = findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (auto* child : std::as_const(children)) {
        if (!headerWidgets.contains(child))
            child->setVisible(visible);
    }
}

void CollapsibleSection::applyCollapsed()
{
    setChildrenVisible(false);
    setMaximumHeight(headerHeight);
}

void CollapsibleSection::setAnimated(bool value)
{
    animated = value;
}

void CollapsibleSection::setCollapsible(bool enabled)
{
    collapsible = enabled;
    update();
}

void CollapsibleSection::setExpanded(bool exp)
{
    if (expanded == exp)
        return;
    expanded = exp;
    update();

    if (!isVisible() || !animated) {
        if (!expanded)
            applyCollapsed();
        else {
            setChildrenVisible(true);
            setMaximumHeight(QWIDGETSIZE_MAX);
            emit sectionExpanded();
        }
        return;
    }

    if (expanded) {
        setChildrenVisible(true);
        setMaximumHeight(QWIDGETSIZE_MAX); // Allow layout to compute proper size
        int target = sizeHint().height();
        if (target <= headerHeight)
            target = headerHeight + 1;
        setMaximumHeight(headerHeight); // Reset for animation start
        animation->setStartValue(headerHeight);
        animation->setEndValue(target);
    } else {
        animation->setStartValue(height());
        animation->setEndValue(headerHeight);
    }
    animation->start();
}

void CollapsibleSection::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Header background
    p.fillRect(0, 0, width(), headerHeight, headerBg);

    // Arrow glyph (only when collapsible)
    int titleLeft = 8;
    if (collapsible) {
        p.setPen(Qt::white);
        QFont af = font();
        af.setPointSizeF(af.pointSizeF() * 0.85);
        p.setFont(af);
        p.drawText(QRect(8, 0, 18, headerHeight), Qt::AlignVCenter | Qt::AlignHCenter,
                   expanded ? QStringLiteral("▼") : QStringLiteral("▶"));
        titleLeft = 30;
    }

    // Title — leave room for header widgets on the right
    int titleRight = width() - 4;
    for (auto* w : std::as_const(headerWidgets))
        titleRight -= (w->width() + 4);
    QFont tf = font();
    tf.setBold(true);
    p.setPen(Qt::white);
    p.setFont(tf);
    p.drawText(QRect(titleLeft, 0, titleRight - titleLeft, headerHeight), Qt::AlignVCenter | Qt::AlignLeft, title());

    // Subtle border around content area
    p.setPen(QPen(frameBorder, 1));
    p.drawRect(0, headerHeight, width() - 1, height() - headerHeight - 1);
}

void CollapsibleSection::keyPressEvent(QKeyEvent* event)
{
    if (collapsible && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space)) {
        setExpanded(!expanded);
        event->accept();
        return;
    }
    QGroupBox::keyPressEvent(event);
}

void CollapsibleSection::mousePressEvent(QMouseEvent* event)
{
    if (!collapsible) {
        QGroupBox::mousePressEvent(event);
        return;
    }
    if (event->position().y() <= headerHeight)
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
    setAccessibleName(title);
    setContentsMargins(2, headerHeight + 4, 2, 4);
}

void CollapsibleSection::changeEvent(QEvent* event)
{
    QGroupBox::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        setContentsMargins(2, headerHeight + 4, 2, 4);
}

void CollapsibleSection::showEvent(QShowEvent* event)
{
    // Ensure margins are correct at first show (calculateFrame may have run
    // between init() and here via setTitle called from .ui setup).
    setContentsMargins(2, headerHeight + 4, 2, 4);
    QGroupBox::showEvent(event);
    repositionHeaderWidgets();
}

void CollapsibleSection::resizeEvent(QResizeEvent* event)
{
    QGroupBox::resizeEvent(event);
    repositionHeaderWidgets();
}
