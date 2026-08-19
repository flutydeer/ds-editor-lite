#include "AppearancePage.h"

#include <QLocale>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <QAbstractItemView>
#include <QFontDatabase>

#include "Model/AppOptions/AppOptions.h"
#include "Utils/FontManager.h"
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include "UI/Dialogs/Base/RestartDialog.h"
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Theme/ThemeIds.h>

AppearancePage::AppearancePage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void AppearancePage::modifyOption() {
    const auto option = appOptions->appearance();
    option->useNativeFrame = m_swUseNativeFrame->value();
#if defined(WITH_DIRECT_MANIPULATION)
    option->enableDirectManipulation = m_swEnableDirectManipulation->value();
#endif
    option->animationEnabled = m_swAnimationEnabled->value();
    option->animationTimeScale = QLocale().toDouble(m_leAnimationTimeScale->text());
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

void AppearancePage::changeInterfaceFont(const int index) {
    // Index 0 is the "Default" item (empty family restores platform default).
    const auto family = index <= 0 ? QString() : m_cbxInterfaceFont->itemData(index).toString();
    appOptions->appearance()->uiFontFamily = family;
    appOptions->saveAndNotify(AppOptionsGlobal::Appearance);
}

QWidget *AppearancePage::createContentWidget() {
    const auto widget = new QWidget;
    const auto option = appOptions->appearance();

    m_cbxTheme = new ComboBox;
    m_cbxTheme->addItem(tr("System"), ThemeIds::systemThemePreferenceId());
    m_cbxTheme->addItem(tr("Light"), ThemeIds::lightThemePreferenceId());
    m_cbxTheme->addItem(tr("Dark"), ThemeIds::darkThemePreferenceId());
    m_cbxTheme->setCurrentIndex(m_cbxTheme->findData(option->themeId));
    connect(m_cbxTheme, &ComboBox::currentIndexChanged, this, &AppearancePage::changeTheme);

    const auto themeCard = new OptionListCard(tr("Theme"));
    themeCard->addItem(tr("Color theme"), m_cbxTheme);

    m_cbxInterfaceFont = new ComboBox;
    // Item 0 is "Default" (restores the platform-default UI font).
    m_cbxInterfaceFont->addItem(tr("Default"), QString());
    m_fontFamilies = QFontDatabase::families();
    // Windows reserves the "System" font family for its own UI; selecting it
    // as a regular app font has no visible effect. Filter it out to keep the
    // list usable and to avoid confusion with the theme's "System" option.
    m_fontFamilies.removeAll(QStringLiteral("System"));
    for (const auto &family : m_fontFamilies)
        m_cbxInterfaceFont->addItem(family, family);
    const auto savedFamily = option->uiFontFamily;
    if (savedFamily.isEmpty() || !m_fontFamilies.contains(savedFamily))
        m_cbxInterfaceFont->setCurrentIndex(0);
    else
        m_cbxInterfaceFont->setCurrentIndex(m_cbxInterfaceFont->findData(savedFamily));
    // Pixel-per-pixel scrolling for the long font list.
    m_cbxInterfaceFont->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_cbxInterfaceFont, &ComboBox::currentIndexChanged, this,
            &AppearancePage::changeInterfaceFont);

    const auto fontCard = new OptionListCard(tr("Font"));
    fontCard->addItem(tr("Interface font"), m_cbxInterfaceFont);

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

    m_swAnimationEnabled = new SwitchButton(option->animationEnabled);
    connect(m_swAnimationEnabled, &SwitchButton::toggled, this, &AppearancePage::modifyOption);

    m_leAnimationTimeScale = new LineEdit;
    const auto doubleValidator = new QDoubleValidator(m_leAnimationTimeScale);
    m_leAnimationTimeScale->setValidator(doubleValidator);
    m_leAnimationTimeScale->setText(QLocale().toString(option->animationTimeScale));
    m_leAnimationTimeScale->setFixedWidth(80);
    connect(m_leAnimationTimeScale, &LineEdit::editingFinished, this,
            &AppearancePage::modifyOption);

    const auto animationCard = new OptionListCard(tr("Animation"));
    animationCard->addItem(tr("Enable animations"), m_swAnimationEnabled);
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
    mainLayout->addWidget(fontCard);
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
