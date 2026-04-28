//
// Created by FlutyDeer on 2025/9/22.
//

#include "IOptionPage.h"

#include <QScrollArea>

// #include "UI/Controls/OverlayScrollBar.h"

IOptionPage::IOptionPage(QWidget *parent) : QScrollArea(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setWidgetResizable(true);
    // setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void IOptionPage::initializePage() {
    const auto widget = createContentWidget();
    widget->setObjectName("IOptionPageWidget");
    setWidget(widget);
    // OverlayScrollBar::install(this, Qt::Vertical);
}