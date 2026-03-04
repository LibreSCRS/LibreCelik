// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QGroupBox>
#include <QList>
#include <QPropertyAnimation>

class CollapsibleSection : public QGroupBox
{
    Q_OBJECT
    Q_PROPERTY(int sectionHeight READ sectionHeight WRITE setSectionHeight)

public:
    explicit CollapsibleSection(QWidget* parent = nullptr);
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    bool isExpanded() const { return expanded; }
    void setExpanded(bool exp);

    // Property target for QPropertyAnimation
    int sectionHeight() const { return maximumHeight(); }
    void setSectionHeight(int h) { setMaximumHeight(h); }

    // Shadow (not virtual) — QGroupBox::setTitle calls calculateFrame() which
    // resets our content margins on macOS; restore them here.
    void setTitle(const QString& title);

    // Place a widget in the header bar (right-aligned, always visible).
    void addHeaderWidget(QWidget* w);

    // Override the painted header height (default 30). Call before show().
    void setHeaderHeight(int h);

signals:
    void sectionExpanded();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void init();
    void applyCollapsed();
    void setChildrenVisible(bool visible);
    void repositionHeaderWidgets();

    bool expanded = true;
    int expandedHeight = -1;
    QPropertyAnimation* animation = nullptr;
    QList<QWidget*> headerWidgets_;

    static constexpr int HEADER_HEIGHT = 30;
    int headerHeight_ = HEADER_HEIGHT;
};
