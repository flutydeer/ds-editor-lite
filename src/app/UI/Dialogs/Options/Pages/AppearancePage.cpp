//
// Created by fluty on 24-3-16.
//

#include "AppearancePage.h"

#include <QVBoxLayout>

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Controls/SwitchButton.h"

AppearancePage::AppearancePage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->appearance();

    m_swUseNativeFrame = new SwitchButton(option->useNativeFrame);
    auto useNativeFrameItem = new OptionsCardItem;
    useNativeFrameItem->setTitle(tr("Use native frame"));
    useNativeFrameItem->setDescription(tr("Restart required"));
    useNativeFrameItem->addWidget(m_swUseNativeFrame);
    connect(m_swUseNativeFrame, &SwitchButton::toggled, this, &AppearancePage::modifyOption);

    auto windowCardLayout = new QVBoxLayout;
    windowCardLayout->addWidget(useNativeFrameItem);
    windowCardLayout->setContentsMargins(10, 5, 10, 5);
    windowCardLayout->setSpacing(0);

    auto windowCard = new OptionsCard;
    windowCard->setTitle(tr("Window"));
    windowCard->card()->setLayout(windowCardLayout);

    m_cbxAnimationLevel = new ComboBox;
    m_cbxAnimationLevel->addItems(animationLevelsName);
    m_cbxAnimationLevel->setCurrentIndex(option->animationLevel);
    connect(m_cbxAnimationLevel, &ComboBox::currentIndexChanged, this,
            &AppearancePage::modifyOption);

    m_leAnimationTimeScale = new LineEdit;
    auto doubleValidator = new QDoubleValidator(m_leAnimationTimeScale);
    m_leAnimationTimeScale->setValidator(doubleValidator);
    m_leAnimationTimeScale->setText(QString::number(option->animationTimeScale));
    m_leAnimationTimeScale->setFixedWidth(80);
    connect(m_leAnimationTimeScale, &LineEdit::editingFinished, this,
            &AppearancePage::modifyOption);

    auto animationLevelItem = new OptionsCardItem;
    animationLevelItem->setTitle(tr("Level"));
    animationLevelItem->setDescription(tr("Choose an animation level that suitable for you"));
    animationLevelItem->addWidget(m_cbxAnimationLevel);

    auto animationTimeScaleItem = new OptionsCardItem;
    animationTimeScaleItem->setTitle(tr("Time scale"));
    animationTimeScaleItem->setDescription(tr("Adjust animations' duration"));
    animationTimeScaleItem->addWidget(m_leAnimationTimeScale);

    auto animationCardLayout = new QVBoxLayout;
    animationCardLayout->addWidget(animationLevelItem);
    animationCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    animationCardLayout->addWidget(animationTimeScaleItem);
    animationCardLayout->setContentsMargins(10, 5, 10, 5);
    animationCardLayout->setSpacing(0);

    auto animationCard = new OptionsCard;
    animationCard->setTitle(tr("Animation"));
    animationCard->card()->setLayout(animationCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(windowCard);
    mainLayout->addWidget(animationCard);
    mainLayout->addSpacerItem(
        new QSpacerItem(8, 4, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}
void AppearancePage::modifyOption() {
    auto option = appOptions->appearance();
    option->useNativeFrame = m_swUseNativeFrame->value();
    option->animationLevel =
        static_cast<AnimationGlobal::AnimationLevels>(m_cbxAnimationLevel->currentIndex());
    option->animationTimeScale = m_leAnimationTimeScale->text().toDouble();
    appOptions->saveAndNotify();
}