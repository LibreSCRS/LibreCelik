// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "collapsiblesection.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>

static const QColor HEADER_BG{61, 140, 149}; // teal
static const QColor FRAME_BORDER{61, 140, 149, 80};

CollapsibleSection::CollapsibleSection(QWidget* parent) : QGroupBox(parent)
{
    init();
}

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent) : QGroupBox(title, parent)
{
    init();
}

void CollapsibleSection::setHeaderHeight(int h)
{
    headerHeight_ = h;
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
    setContentsMargins(2, headerHeight_ + 4, 2, 4);

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
    });
}

void CollapsibleSection::addHeaderWidget(QWidget* w)
{
    w->setParent(this);
    w->setFixedHeight(22);
    if (auto* btn = qobject_cast<QPushButton*>(w)) {
        btn->setStyleSheet("QPushButton {"
                           "  color: white;"
                           "  background-color: rgb(72, 148, 156);"
                           "  border: 1px solid rgb(100, 168, 176);"
                           "  border-radius: 3px;"
                           "  padding: 0px 6px;"
                           "  font-size: 11px;"
                           "}"
                           "QPushButton:hover { background-color: rgb(82, 158, 166); }"
                           "QPushButton:pressed { background-color: rgb(50, 120, 130); }"
                           "QPushButton:disabled { color: rgb(160, 205, 210); border-color: rgb(90, 155, 162); }");
    }
    headerWidgets_.append(w);
    repositionHeaderWidgets();
    w->show();
}

void CollapsibleSection::repositionHeaderWidgets()
{
    if (headerWidgets_.isEmpty())
        return;
    int x = width() - 4;
    for (int i = headerWidgets_.size() - 1; i >= 0; --i) {
        QWidget* w = headerWidgets_[i];
        int ww = w->sizeHint().width();
        if (ww < 1)
            ww = w->width();
        x -= ww + 4;
        w->setGeometry(x, (headerHeight_ - w->height()) / 2, ww, w->height());
        w->raise();
    }
    update();
}

void CollapsibleSection::setChildrenVisible(bool visible)
{
    auto children = findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
    for (auto* child : std::as_const(children)) {
        if (!headerWidgets_.contains(child))
            child->setVisible(visible);
    }
}

void CollapsibleSection::applyCollapsed()
{
    expandedHeight = sizeHint().height();
    setChildrenVisible(false);
    setMaximumHeight(headerHeight_);
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
        int target = (expandedHeight > headerHeight_) ? expandedHeight : sizeHint().height();
        animation->setStartValue(headerHeight_);
        animation->setEndValue(target);
    } else {
        expandedHeight = height();
        animation->setStartValue(height());
        animation->setEndValue(headerHeight_);
    }
    animation->start();
}

void CollapsibleSection::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // Header background
    p.fillRect(0, 0, width(), headerHeight_, HEADER_BG);

    // Arrow glyph
    p.setPen(Qt::white);
    QFont af = font();
    af.setPointSizeF(af.pointSizeF() * 0.85);
    p.setFont(af);
    p.drawText(QRect(8, 0, 18, headerHeight_), Qt::AlignVCenter | Qt::AlignHCenter,
               expanded ? QStringLiteral("▼") : QStringLiteral("▶"));

    // Title — leave room for header widgets on the right
    int titleRight = width() - 4;
    for (auto* w : std::as_const(headerWidgets_))
        titleRight -= (w->width() + 4);
    QFont tf = font();
    tf.setBold(true);
    p.setFont(tf);
    p.drawText(QRect(30, 0, titleRight - 30, headerHeight_), Qt::AlignVCenter | Qt::AlignLeft, title());

    // Subtle border around content area
    p.setPen(QPen(FRAME_BORDER, 1));
    p.drawRect(0, headerHeight_, width() - 1, height() - headerHeight_ - 1);
}

void CollapsibleSection::mousePressEvent(QMouseEvent* event)
{
    if (event->position().y() <= headerHeight_)
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
    setContentsMargins(2, headerHeight_ + 4, 2, 4);
}

void CollapsibleSection::changeEvent(QEvent* event)
{
    QGroupBox::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        setContentsMargins(2, headerHeight_ + 4, 2, 4);
}

void CollapsibleSection::showEvent(QShowEvent* event)
{
    // Ensure margins are correct at first show (calculateFrame may have run
    // between init() and here via setTitle called from .ui setup).
    setContentsMargins(2, headerHeight_ + 4, 2, 4);
    QGroupBox::showEvent(event);
    repositionHeaderWidgets();
}

void CollapsibleSection::resizeEvent(QResizeEvent* event)
{
    QGroupBox::resizeEvent(event);
    repositionHeaderWidgets();
}
