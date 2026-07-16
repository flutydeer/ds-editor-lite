//
// Created by FlutyDeer on 2025/9/22.
//

#include "IOptionPage.h"

#include <QEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

// #include "UI/Controls/OverlayScrollBar.h"

IOptionPage::IOptionPage(QWidget *parent) : QScrollArea(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setWidgetResizable(true);
}

void IOptionPage::initializePage() {
    const auto widget = createContentWidget();
    widget->setObjectName("IOptionPageWidget");
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
