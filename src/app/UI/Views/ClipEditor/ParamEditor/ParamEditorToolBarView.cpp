//
// Created by fluty on 24-9-16.
//

#include "ParamEditorToolBarView.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "Utils/ParamUtils.h"
#include "SpeakerMixToolBarView.h"

#include <QHBoxLayout>
#include <QLabel>

ParamEditorToolBarView::ParamEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    lbForegroundParam = new QLabel(tr("Foreground:"));
    lbForegroundParam->setObjectName("lbForegroundParam");

    cbForegroundParam = new ComboBox(true);
    cbForegroundParam->setObjectName("cbForegroundParam");
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0); // Remove pitch
    cbForegroundParam->addItem(tr("Speaker Mix"));

    const auto btnSwap = new Button(tr("Swap"));
    btnSwap->setObjectName("btnSwap");

    lbBackgroundParam = new QLabel(tr("Background:"));
    lbBackgroundParam->setObjectName("lbBackgroundParam");

    cbBackgroundParam = new ComboBox(true);
    cbBackgroundParam->setObjectName("cbBackgroundParam");
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0); // Remove pitch

    m_speakerMixToolBar = new SpeakerMixToolBarView;
    m_speakerMixToolBar->setVisible(false);

    const auto layout = new QHBoxLayout();
    layout->addSpacing(64);
    layout->addWidget(lbForegroundParam);
    layout->addWidget(cbForegroundParam);
    layout->addWidget(btnSwap);
    layout->addWidget(lbBackgroundParam);
    layout->addWidget(cbBackgroundParam);
    layout->addWidget(m_speakerMixToolBar);
    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, 4, 4, 4);

    setLayout(layout);
    // setFixedHeight(32);

    connect(cbForegroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onForegroundSelectionChanged);
    connect(cbBackgroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onBackgroundSelectionChanged);
    connect(btnSwap, &Button::clicked, this, &ParamEditorToolBarView::onSwap);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::previousKeyframe, this,
            &ParamEditorToolBarView::previousKeyframe);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::nextKeyframe, this,
            &ParamEditorToolBarView::nextKeyframe);

    cbForegroundParam->setCurrentIndex(appOptions->general()->defaultForegroundParam - 1);
    cbBackgroundParam->setCurrentIndex(appOptions->general()->defaultBackgroundParam - 1);
}

void ParamEditorToolBarView::setSpeakerMixMode(bool on) {
    m_speakerMixToolBar->setVisible(on);
}

void ParamEditorToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
    m_speakerMixToolBar->setSpeakers(names, colors);
}

void ParamEditorToolBarView::onForegroundSelectionChanged(const int index) {
    emit foregroundChanged(static_cast<ParamInfo::Name>(index + 1));
}

void ParamEditorToolBarView::onBackgroundSelectionChanged(const int index) {
    emit backgroundChanged(static_cast<ParamInfo::Name>(index + 1));
}

void ParamEditorToolBarView::onSwap() const {
    const int fgIndex = cbForegroundParam->currentIndex();
    const int speakerMixIndex = cbForegroundParam->count() - 1;
    if (fgIndex == speakerMixIndex)
        return;
    const int temp = fgIndex;
    cbForegroundParam->setCurrentIndex(cbBackgroundParam->currentIndex());
    cbBackgroundParam->setCurrentIndex(temp);
}
