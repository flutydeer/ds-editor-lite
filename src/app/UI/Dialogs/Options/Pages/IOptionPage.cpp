#include "IOptionPage.h"

#include <QEvent>
#include <QLayout>
#include <QMargins>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/SmoothScroller.h>

QMargins IOptionPage::contentMargins() const {
    // Left/right page-content spacing lives here, not in the QSS box model:
    // padding on the QScrollArea would inset the viewport and drag the overlay
    // scrollbar away from the right edge. Card spacing is the outer layout's
    // job too (cards carry no bottom margin of their own).
    return QMargins(16, 16, 16, 16);
}

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
    if (auto *layout = widget->layout()) {
        layout->setContentsMargins(contentMargins());
        layout->setSpacing(12);
    }
    setWidget(widget);
    syncContentMinimum();
}

void IOptionPage::syncContentMinimum() {
    // A page whose layout has heightForWidth (word-wrapped option
    // descriptions) reports a minimumSizeHint taller than its real
    // heightForWidth at the viewport width: a wordWrap QLabel wraps its text
    // at a heuristic width, so the unwrapped minimum wins. QScrollArea sizes a
    // resizable page from that minimum (updateScrollBars: p.expandedTo(min)),
    // which would leave a permanent unpainted gap below the last card. Pinning
    // the explicit minimum lets the page height come from heightForWidth
    // alone; the resize above still keeps it at least as tall as the viewport.
    // Same fix as the package manager's details page.
    //
    // The pin must follow the layout's CURRENT state: pages that swap their
    // visible sub-pages (Automation: access control <-> toolset) can end up
    // with no hfw item visible at all, and then the scroll area has no
    // heightForWidth to fall back on - keeping the pin would size the page to
    // the viewport alone and squeeze everything inside it. Release the pin in
    // that case so the layout's own minimum drives the height again.
    auto *content = widget();
    if (!content || !content->layout())
        return;
    content->setMinimumHeight(content->layout()->hasHeightForWidth() ? 1 : 0);
}

void IOptionPage::refitContent() {
    // Re-run the scroll area's page sizing after the visible content changed:
    // toggling widgetResizable re-runs QScrollAreaPrivate::updateScrollBars,
    // which recomputes the page height (and the scroll range) from the current
    // size hints.
    if (!widget())
        return;
    syncContentMinimum();
    setWidgetResizable(false);
    setWidgetResizable(true);
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
