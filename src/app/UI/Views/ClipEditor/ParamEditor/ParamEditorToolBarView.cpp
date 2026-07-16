//
// Created by fluty on 24-9-16.
//

#include "ParamEditorToolBarView.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "Utils/ParamUtils.h"
#include "SpeakerMixToolBarView.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

ParamEditorToolBarView::ParamEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    lbForegroundParam = new QLabel(tr("Foreground:"));
    lbForegroundParam->setObjectName("lbForegroundParam");

    cbForegroundParam = new ComboBox(true);
    cbForegroundParam->setObjectName("cbForegroundParam");
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0); // Remove pitch

    m_btnSwap = new Button(tr("Swap"));
    m_btnSwap->setObjectName("btnSwap");

    lbBackgroundParam = new QLabel(tr("Background:"));
    lbBackgroundParam->setObjectName("lbBackgroundParam");

    cbBackgroundParam = new ComboBox(true);
    cbBackgroundParam->setObjectName("cbBackgroundParam");
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0);                              // Remove pitch
    cbBackgroundParam->removeItem(cbBackgroundParam->count() - 1); // Remove speaker mix

    m_speakerMixToolBar = new SpeakerMixToolBarView;
    m_speakerMixToolBar->setVisible(false);

    const auto layout = new QHBoxLayout();
    layout->addSpacing(64);
    layout->addWidget(lbForegroundParam);
    layout->addWidget(cbForegroundParam);
    layout->addWidget(m_btnSwap);
    layout->addWidget(lbBackgroundParam);
    layout->addWidget(cbBackgroundParam);
    layout->addWidget(m_speakerMixToolBar);
    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, 4, 4, 4);

    setLayout(layout);

    connect(cbForegroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onForegroundSelectionChanged);
    connect(cbBackgroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onBackgroundSelectionChanged);
    connect(m_btnSwap, &Button::clicked, this, &ParamEditorToolBarView::onSwap);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::previousKeyframe, this,
            &ParamEditorToolBarView::previousKeyframe);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::nextKeyframe, this,
            &ParamEditorToolBarView::nextKeyframe);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::bypassDynamicMix, this,
            &ParamEditorToolBarView::bypassDynamicMix);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::resumeDynamicMix, this,
            &ParamEditorToolBarView::resumeDynamicMix);
    connect(m_speakerMixToolBar, &SpeakerMixToolBarView::stopDynamicMix, this,
            &ParamEditorToolBarView::stopDynamicMix);

    cbForegroundParam->setCurrentIndex(appOptions->general()->defaultForegroundParam - 1);
    cbBackgroundParam->setCurrentIndex(appOptions->general()->defaultBackgroundParam - 1);
}

void ParamEditorToolBarView::setSpeakerMixMode(bool on) {
    m_speakerMixToolBar->setVisible(on);
}

void ParamEditorToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
    m_speakerMixToolBar->setSpeakers(names, colors);
}

void ParamEditorToolBarView::setSpeakerMixDynamicState(const SpeakerMixDynamicUiState state) {
    m_speakerMixToolBar->setDynamicState(state);
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

void ParamEditorToolBarView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void ParamEditorToolBarView::retranslateUi() {
    lbForegroundParam->setText(tr("Foreground:"));
    lbBackgroundParam->setText(tr("Background:"));
    m_btnSwap->setText(tr("Swap"));

    const auto foregroundIndex = cbForegroundParam->currentIndex();
    const QSignalBlocker foregroundBlocker(cbForegroundParam);
    cbForegroundParam->clear();
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0);
    cbForegroundParam->setCurrentIndex(foregroundIndex);

    const auto backgroundIndex = cbBackgroundParam->currentIndex();
    const QSignalBlocker backgroundBlocker(cbBackgroundParam);
    cbBackgroundParam->clear();
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0);
    cbBackgroundParam->removeItem(cbBackgroundParam->count() - 1);
    cbBackgroundParam->setCurrentIndex(backgroundIndex);
}
