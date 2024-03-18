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
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"

#include <QPushButton>

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
    animationLevelItem->setTitle("等级");
    animationLevelItem->setDescription("选择让你感到舒适的动画强度");
    animationLevelItem->addWidget(m_cbxAnimationLevel);

    auto animationTimeScaleItem = new OptionsCardItem;
    animationTimeScaleItem->setTitle("时间缩放");
    animationTimeScaleItem->addWidget(m_leAnimationTimeScale);

    auto animationCardLayout = new QVBoxLayout;
    animationCardLayout->addWidget(animationLevelItem);
    animationCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    animationCardLayout->addWidget(animationTimeScaleItem);
    animationCardLayout->setContentsMargins({});
    animationCardLayout->setSpacing(0);

    auto animationCard = new OptionsCard;
    animationCard->setTitle(tr("动画"));
    animationCard->card()->setLayout(animationCardLayout);

    auto testCardLayout = new QVBoxLayout;
    auto item1 = new OptionsCardItem;
    item1->setTitle("Test1");
    // item1->addWidget(new QPushButton("Test"));
    // item1->addWidget(new QPushButton("Test"));
    item1->setCheckable(true);
    testCardLayout->addWidget(item1);
    testCardLayout->setSpacing(0);
    testCardLayout->setContentsMargins({});

    auto testCard = new OptionsCard;
    testCard->setTitle("测试");
    testCard->card()->setLayout(testCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(animationCard);
    mainLayout->addWidget(testCard);
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