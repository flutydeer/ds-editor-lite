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
