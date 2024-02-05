//
// Created by fluty on 2024/2/5.
//

#include "ActionButtonsView.h"

#include <QHBoxLayout>

ActionButtonsView::ActionButtonsView(QWidget *parent) {
    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);
    setLayout(mainLayout);
    setContentsMargins({});
}