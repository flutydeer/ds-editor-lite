#include "IOptionPage.h"

#include <QEvent>
#include <QLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/SmoothScroller.h>

IOptionPage::IOptionPage(QWidget *parent) : QScrollArea(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setWidgetResizable(true);
    // Overlay scrollbar: the native bar is disabled (no space reserved), so
    // scrollbar visibility never changes the content/card width.
    OverlayScrollBar::install(this, Qt::Vertical);
    // Animate mouse-wheel scrollbar movement with OutCubic; touchpad passes through (see
    // SmoothScroller).
    auto *smoothScroller = new SmoothScroller(this);
    smoothScroller->attachTo(this);
}

void IOptionPage::initializePage() {
    const auto widget = createContentWidget();
    widget->setObjectName("IOptionPageWidget");
    // Left/right page-content spacing lives here, not in the QSS box model:
    // padding on the QScrollArea would inset the viewport and drag the
    // overlay scrollbar away from the right edge. Card spacing is the outer
    // layout's job too (cards carry no bottom margin of their own).
    if (auto *layout = widget->layout()) {
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        // Pages whose content has heightForWidth (word-wrapped option
        // descriptions) report a minimumSizeHint taller than their real
        // heightForWidth at the viewport width: a wordWrap QLabel wraps its
        // text at a heuristic width, so the unwrapped minimum wins.
        // QScrollArea sizes a resizable page from that minimum
        // (updateScrollBars: p.expandedTo(min)), which would leave a permanent
        // unpainted gap below the last card. Pinning the explicit minimum lets
        // the page height come from heightForWidth alone; the resize above
        // still keeps it at least as tall as the viewport. Same fix as the
        // package manager's details page.
        if (layout->hasHeightForWidth())
            widget->setMinimumHeight(1);
    }
    setWidget(widget);
}

void IOptionPage::changeEvent(QEvent *event) {
    QScrollArea::changeEvent(event);
    if (event->type() != QEvent::LanguageChange || m_retranslatePending)
        return;

    m_retranslatePending = true;
    QTimer::singleShot(0, this, [this] {
        const auto scrollPosition = verticalScrollBar()->value();
        modifyOption();
        initializePage();
        verticalScrollBar()->setValue(scrollPosition);
        m_retranslatePending = false;
    });
}
