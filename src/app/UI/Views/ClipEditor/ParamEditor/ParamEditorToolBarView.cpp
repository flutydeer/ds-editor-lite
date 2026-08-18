#include "ParamEditorToolBarView.h"

#include "Model/AppOptions/AppOptions.h"
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/IconLabel.h>
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Model/Utils/ParamUtils.h"
#include "ParamEditToolBarView.h"
#include "SpeakerMixToolBarView.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStackedWidget>

ParamEditorToolBarView::ParamEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(ClipEditorGlobal::paramEditorToolBarHeight);

    lbForegroundParam = new IconLabel;
    lbForegroundParam->setObjectName("lbForegroundParam");
    lbForegroundParam->setToolTip(tr("Foreground"));
    lbForegroundParam->setIcon(QStringLiteral(":/svg/icons/edit_16_regular.svg"));
    lbForegroundParam->setColorToken(QStringLiteral("text.secondary"));

    cbForegroundParam = new ComboBox(WheelEventPolicy::Handle);
    cbForegroundParam->setObjectName("cbForegroundParam");
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0); // Remove pitch

    m_btnSwap = new ToolButton;
    m_btnSwap->setObjectName("btnSwap");
    m_btnSwap->setActionIcon(QStringLiteral(":/svg/icons/arrow_swap_20_regular.svg"),
                             QSize(20, 20));
    m_btnSwap->setActionIconHoverColor(
        ThemeManager::instance()->semanticColor(QStringLiteral("text.emphasis")));
    m_btnSwap->setToolTip(tr("Swap"));
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_btnSwap->setActionIconHoverColor(
            ThemeManager::instance()->semanticColor(QStringLiteral("text.emphasis")));
    });

    lbBackgroundParam = new IconLabel;
    lbBackgroundParam->setObjectName("lbBackgroundParam");
    lbBackgroundParam->setToolTip(tr("Background"));
    lbBackgroundParam->setIcon(QStringLiteral(":/svg/icons/eye_16_regular.svg"));
    lbBackgroundParam->setColorToken(QStringLiteral("text.secondary"));

    cbBackgroundParam = new ComboBox(WheelEventPolicy::Handle);
    cbBackgroundParam->setObjectName("cbBackgroundParam");
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0);                              // Remove pitch
    cbBackgroundParam->removeItem(cbBackgroundParam->count() - 1); // Remove speaker mix

    m_paramEditToolBar = new ParamEditToolBarView;
    m_speakerMixToolBar = new SpeakerMixToolBarView;
    m_toolBarStack = new QStackedWidget;
    m_toolBarStack->setObjectName("paramEditorToolBarStack");
    m_toolBarStack->addWidget(m_paramEditToolBar);
    m_toolBarStack->addWidget(m_speakerMixToolBar);
    m_toolBarStack->setCurrentWidget(m_paramEditToolBar);
    m_toolBarStack->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);

    lbForegroundParam->setMaximumHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    cbForegroundParam->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    m_btnSwap->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    lbBackgroundParam->setMaximumHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    cbBackgroundParam->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);

    const auto layout = new QHBoxLayout();
    layout->addSpacing(64);
    layout->addWidget(lbForegroundParam);
    layout->addWidget(cbForegroundParam);
    layout->addWidget(m_btnSwap);
    layout->addWidget(lbBackgroundParam);
    layout->addWidget(cbBackgroundParam);
    layout->addWidget(m_toolBarStack);
    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, ClipEditorGlobal::paramEditorToolBarVerticalMargin, 4,
                               ClipEditorGlobal::paramEditorToolBarVerticalMargin);

    setLayout(layout);

    connect(cbForegroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onForegroundSelectionChanged);
    connect(cbBackgroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onBackgroundSelectionChanged);
    connect(m_btnSwap, &QPushButton::clicked, this, &ParamEditorToolBarView::onSwap);
    connect(m_paramEditToolBar, &ParamEditToolBarView::editModeChanged, this,
            &ParamEditorToolBarView::editModeChanged);
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
    setSpeakerMixMode(static_cast<ParamInfo::Name>(cbForegroundParam->currentIndex() + 1) ==
                      ParamInfo::SpeakerMix);
    retranslateUi();
}

void ParamEditorToolBarView::setSpeakerMixMode(bool on) {
    m_toolBarStack->setCurrentWidget(on ? static_cast<QWidget *>(m_speakerMixToolBar)
                                        : static_cast<QWidget *>(m_paramEditToolBar));
}

void ParamEditorToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
    m_speakerMixToolBar->setSpeakers(names, colors);
}

void ParamEditorToolBarView::setSpeakerMixDynamicState(const SpeakerMixDynamicUiState state) {
    m_speakerMixToolBar->setDynamicState(state);
}

void ParamEditorToolBarView::onForegroundSelectionChanged(const int index) {
    const auto name = static_cast<ParamInfo::Name>(index + 1);
    setSpeakerMixMode(name == ParamInfo::SpeakerMix);
    emit foregroundChanged(name);
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
    lbForegroundParam->setToolTip(tr("Foreground"));
    lbBackgroundParam->setToolTip(tr("Background"));
    m_btnSwap->setToolTip(tr("Swap"));

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
