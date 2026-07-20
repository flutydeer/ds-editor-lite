//
// Created by fluty on 24-3-16.
//

#include "AppearancePage.h"

#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/SwitchButton.h"
#include "UI/Dialogs/Base/RestartDialog.h"
#include "UI/Utils/Theme/ThemeLoader.h"
#include "UI/Utils/ThemeManager.h"

AppearancePage::AppearancePage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void AppearancePage::modifyOption() {
    const auto option = appOptions->appearance();
    option->useNativeFrame = m_swUseNativeFrame->value();
#if defined(WITH_DIRECT_MANIPULATION)
    option->enableDirectManipulation = m_swEnableDirectManipulation->value();
#endif
    option->animationLevel =
        static_cast<AnimationGlobal::AnimationLevels>(m_cbxAnimationLevel->currentIndex());
    option->animationTimeScale = m_leAnimationTimeScale->text().toDouble();
    appOptions->saveAndNotify(AppOptionsGlobal::Appearance);
}

void AppearancePage::changeTheme(const int index) {
    const auto themePreferenceId = m_cbxTheme->itemData(index).toString();
    if (themePreferenceId.isEmpty())
        return;

    const auto themeManager = ThemeManager::instance();
    const auto previousThemePreferenceId = appOptions->appearance()->themeId;
    if (themePreferenceId != previousThemePreferenceId &&
        !themeManager->applyThemePreference(themePreferenceId)) {
        const QSignalBlocker blocker(m_cbxTheme);
        m_cbxTheme->setCurrentIndex(m_cbxTheme->findData(previousThemePreferenceId));
        QMessageBox::critical(this, tr("Theme switch failed"), ThemeLoader::lastError());
        return;
    }

    appOptions->appearance()->themeId = themePreferenceId;
    appOptions->saveAndNotify(AppOptionsGlobal::Appearance);
}

QWidget *AppearancePage::createContentWidget() {
    const auto widget = new QWidget;
    const auto option = appOptions->appearance();

    m_cbxTheme = new ComboBox;
    m_cbxTheme->addItem(tr("System"), AppearanceOption::systemThemePreferenceId());
    m_cbxTheme->addItem(tr("Light"), AppearanceOption::lightThemePreferenceId());
    m_cbxTheme->addItem(tr("Dark"), AppearanceOption::darkThemePreferenceId());
    m_cbxTheme->setCurrentIndex(m_cbxTheme->findData(option->themeId));
    connect(m_cbxTheme, &ComboBox::currentIndexChanged, this, &AppearancePage::changeTheme);

    const auto themeCard = new OptionListCard(tr("Theme"));
    themeCard->addItem(tr("Color theme"), m_cbxTheme);

    m_swUseNativeFrame = new SwitchButton(option->useNativeFrame);
    connect(m_swUseNativeFrame, &SwitchButton::toggled, this, [this] {
        modifyOption();
        const auto message = tr(
            "The settings will take effect after restarting the app. Do you want to restart now?");
        const auto dlg = new RestartDialog(message, true, this);
        dlg->show();
    });

    const auto windowCard = new OptionListCard(tr("Window"));
    windowCard->addItem(tr("Use native frame"), tr("App needs a restart to take effect"),
                        m_swUseNativeFrame);

    m_cbxAnimationLevel = new ComboBox;
    m_cbxAnimationLevel->addItems(animationLevelsName);
    m_cbxAnimationLevel->setCurrentIndex(option->animationLevel);
    connect(m_cbxAnimationLevel, &ComboBox::currentIndexChanged, this,
            &AppearancePage::modifyOption);

    m_leAnimationTimeScale = new LineEdit;
    const auto doubleValidator = new QDoubleValidator(m_leAnimationTimeScale);
    m_leAnimationTimeScale->setValidator(doubleValidator);
    m_leAnimationTimeScale->setText(QString::number(option->animationTimeScale));
    m_leAnimationTimeScale->setFixedWidth(80);
    connect(m_leAnimationTimeScale, &LineEdit::editingFinished, this,
            &AppearancePage::modifyOption);

    const auto animationCard = new OptionListCard(tr("Animation"));
    animationCard->addItem(tr("Level"), m_cbxAnimationLevel);
    animationCard->addItem(tr("Duration scale"), m_leAnimationTimeScale);

#if defined(WITH_DIRECT_MANIPULATION)
    const auto touchCard = new OptionListCard(tr("Touch"));
    m_swEnableDirectManipulation = new SwitchButton(option->enableDirectManipulation);
    connect(m_swEnableDirectManipulation, &SwitchButton::toggled, this,
            &AppearancePage::modifyOption);
    touchCard->addItem(tr("Enable Direct Manipulation"), m_swEnableDirectManipulation);
#endif

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(themeCard);
    mainLayout->addWidget(windowCard);
    mainLayout->addWidget(animationCard);
#if defined(WITH_DIRECT_MANIPULATION)
    mainLayout->addWidget(touchCard);
#endif
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
