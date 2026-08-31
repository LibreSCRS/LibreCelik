// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QColor>
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
    CollapsibleSection(const QString& title, const QColor& headerColor, QWidget* parent = nullptr);

    QColor headerColor() const;
    void setHeaderColor(const QColor& color);

    bool isExpanded() const
    {
        return expanded;
    }
    void setExpanded(bool exp);

    // Property target for QPropertyAnimation
    int sectionHeight() const
    {
        return maximumHeight();
    }
    void setSectionHeight(int h)
    {
        setMaximumHeight(h);
        // Propagate layout invalidation up so that parent layouts
        // (e.g. the scroll area container) recalculate in the same
        // frame — prevents a one-frame bounce in nested sections.
        for (QWidget* p = parentWidget(); p; p = p->parentWidget()) {
            p->updateGeometry();
            if (p->inherits("QAbstractScrollArea"))
                break;
        }
    }

    // Shadow (not virtual) — QGroupBox::setTitle calls calculateFrame() which
    // resets our content margins on macOS; restore them here.
    void setTitle(const QString& title);

    // Place a widget in the header bar (right-aligned, always visible).
    void addHeaderWidget(QWidget* w);

    // Override the painted header height (default 30). Call before show().
    void setHeaderHeight(int h);

    // Disable animation for instant collapse/expand (avoids flicker in nested sections).
    void setAnimated(bool animated);

    // When false, the section is always expanded — no arrow, no toggle on click.
    void setCollapsible(bool enabled);

    // Re-apply the settled expanded/collapsed state to the content.
    //
    // setExpanded() is a no-op when the flag already matches, which is not the
    // same as "the content matches the flag": a caller that rebuilds the
    // section's content adds NEW child widgets, and a layout shows those even
    // when the section around them is closed. Call this after such a rebuild.
    // No-op while an animation is in flight — that animation is already on its
    // way to the settled state.
    void refreshContentVisibility();

signals:
    void sectionExpanded();

    /// Emitted only when the READER toggles the section — a click on the header
    /// or Space/Return on the focused one — and never for a programmatic
    /// setExpanded(). That is the whole distinction: it lets a caller whose
    /// default state is derived tell "the person chose this" from "we chose it
    /// for them", and stop arguing with them about a section they just closed.
    void toggledByUser(bool expanded);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void init();
    void applyCollapsed();
    void setChildrenVisible(bool visible);
    void repositionHeaderWidgets();

    bool expanded = true;
    bool animated = true;
    QPropertyAnimation* animation = nullptr;
    QList<QWidget*> headerWidgets;
    QColor headerBg{61, 140, 149};
    QColor frameBorder{61, 140, 149, 80};

    static constexpr int HEADER_HEIGHT = 30;
    int headerHeight = HEADER_HEIGHT;
    bool collapsible = true;
};
