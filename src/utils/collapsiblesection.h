// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QGroupBox>
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

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void init();
    void applyCollapsed();
    void setChildrenVisible(bool visible);

    bool expanded = true;
    int expandedHeight = -1;
    QPropertyAnimation* animation = nullptr;

    static constexpr int HEADER_HEIGHT = 30;
};
