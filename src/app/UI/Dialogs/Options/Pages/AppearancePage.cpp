//
// Created by fluty on 24-3-16.
//

#include "AppearancePage.h"

#include <QVBoxLayout>

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "Controller/AppOptionsController.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"

AppearancePage::AppearancePage(QWidget *parent) : IOptionPage(parent) {
    auto option = AppOptions::instance()->appearance();
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
    animationLevelItem->setTitle("动画等级");
    animationLevelItem->setDescription("选择让你感到舒适的动画强度");
    animationLevelItem->addWidget(m_cbxAnimationLevel);

    auto animationTimeScaleItem = new OptionsCardItem;
    animationTimeScaleItem->setTitle("动画时间缩放");
    animationTimeScaleItem->addWidget(m_leAnimationTimeScale);

    auto animationCardLayout = new QVBoxLayout;
    animationCardLayout->addWidget(animationLevelItem);
    animationCardLayout->addWidget(animationTimeScaleItem);
    animationCardLayout->setContentsMargins({});

    auto animationCard = new OptionsCard;
    animationCard->setTitle(tr("动画"));
    animationCard->card()->setLayout(animationCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(animationCard);
    mainLayout->addSpacing(1);
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}
void AppearancePage::modifyOption() {
    AppearanceOption option;
    option.animationLevel = static_cast<AnimationGlobal::AnimationLevels>(m_cbxAnimationLevel->currentIndex());
    option.animationTimeScale = m_leAnimationTimeScale->text().toDouble();
    auto controller = AppOptionsController::instance()->appearanceController();
    controller->modifyOption(option);
}