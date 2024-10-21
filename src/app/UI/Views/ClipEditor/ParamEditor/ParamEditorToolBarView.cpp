//
// Created by fluty on 24-9-16.
//

#include "ParamEditorToolBarView.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/DividerLine.h"
#include "Utils/ParamUtils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolBar>

ParamEditorToolBarView::ParamEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    auto lbForegroundParam = new QLabel(tr("Foreground:"));
    auto cbForegroundParam = new ComboBox(true);
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0); // Remove pitch

    auto lbBackgroundParam = new QLabel(tr("Background:"));
    auto cbBackgroundParam = new ComboBox(true);
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0); // Remove pitch

    auto layout = new QHBoxLayout();
    layout->addWidget(lbForegroundParam);
    layout->addWidget(cbForegroundParam);
    layout->addWidget(lbBackgroundParam);
    layout->addWidget(cbBackgroundParam);
    // layout->addWidget(new DividerLine(Qt::Vertical));
    // layout->addWidget(new AccentButton("包络"));
    // layout->addWidget(new Button("实参"));
    // layout->addWidget(new DividerLine(Qt::Vertical));
    // layout->addWidget(new QLabel("Tool Buttons"));
    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, 4, 4, 4);

    setLayout(layout);
    setFixedHeight(36);

    connect(cbForegroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onForegroundSelectionChanged);
    connect(cbBackgroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onBackgroundSelectionChanged);

    cbForegroundParam->setCurrentIndex(appOptions->general()->defaultForegroundParam - 1);
    cbBackgroundParam->setCurrentIndex(appOptions->general()->defaultBackgroundParam - 1);
}

void ParamEditorToolBarView::onForegroundSelectionChanged(int index) {
    emit foregroundChanged(static_cast<ParamInfo::Name>(index + 1));
}

void ParamEditorToolBarView::onBackgroundSelectionChanged(int index) {
    emit backgroundChanged(static_cast<ParamInfo::Name>(index + 1));
}