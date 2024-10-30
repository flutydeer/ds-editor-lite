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
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Controls/SwitchButton.h"
#include "UI/Dialogs/Base/RestartDialog.h"

AppearancePage::AppearancePage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->appearance();

    m_swUseNativeFrame = new SwitchButton(option->useNativeFrame);
    connect(m_swUseNativeFrame, &SwitchButton::toggled, this, [=] {
        modifyOption();
        auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    });

    auto windowCard = new OptionListCard(tr("Window"));
    windowCard->addItem(tr("Use native frame"), tr("App needs a restart to take effect"),
                        m_swUseNativeFrame);

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

    auto animationCard = new OptionListCard(tr("Animation"));
    animationCard->addItem(tr("Level"), m_cbxAnimationLevel);
    animationCard->addItem(tr("Duration scale"), m_leAnimationTimeScale);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(windowCard);
    mainLayout->addWidget(animationCard);
    mainLayout->addStretch();
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