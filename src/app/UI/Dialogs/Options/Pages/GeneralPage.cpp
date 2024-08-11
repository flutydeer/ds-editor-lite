//
// Created by fluty on 24-3-18.
//

#include "GeneralPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <WinUser.h>
#  include <shellapi.h>
#endif

GeneralPage::GeneralPage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->general();

    m_btnOpenConfigFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenConfigFolder, &Button::clicked, this, [=] {
#ifdef Q_OS_WIN
        auto path = QDir::toNativeSeparators(appOptions->configPath());
        ShellExecuteW(NULL, L"open", L"explorer",
                      QString("/select, \"%1\"").arg(path).toStdWString().c_str(), NULL, SW_SHOW);

#else
        auto folder = QFileInfo(appOptions->configPath()).absoluteDir().path();
        QDesktopServices::openUrl(QUrl(folder));
#endif
    });

    auto configFileItem = new OptionsCardItem;
    configFileItem->setTitle(tr("Config File"));
    configFileItem->addWidget(m_btnOpenConfigFolder);

    auto configFileCardLayout = new QVBoxLayout;
    configFileCardLayout->addWidget(configFileItem);
    configFileCardLayout->setContentsMargins(10, 5, 10, 5);
    configFileCardLayout->setSpacing(0);

    auto configFileCard = new OptionsCard;
    configFileCard->setTitle(tr("App Config"));
    configFileCard->card()->setLayout(configFileCardLayout);

    auto langKey = languageKeyFromType(option->defaultSingingLanguage);
    m_cbDefaultSingingLanguage = new LanguageComboBox(langKey);
    connect(m_cbDefaultSingingLanguage, &ComboBox::currentIndexChanged, this,
            &GeneralPage::modifyOption);

    auto defaultSingingLanguageItem = new OptionsCardItem;
    defaultSingingLanguageItem->setTitle(tr("Default Singing Language"));
    defaultSingingLanguageItem->addWidget(m_cbDefaultSingingLanguage);

    m_leDefaultLyric = new LineEdit;
    m_leDefaultLyric->setFixedWidth(80);
    m_leDefaultLyric->setText(option->defaultLyric);
    connect(m_leDefaultLyric, &LineEdit::editingFinished, this, &GeneralPage::modifyOption);
    // m_leDefaultLyric->setPlaceholderText(option->defaultLyric);

    auto defaultLyricItem = new OptionsCardItem;
    defaultLyricItem->setTitle(tr("Default Lyric"));
    defaultLyricItem->addWidget(m_leDefaultLyric);

    auto singingCardLayout = new QVBoxLayout;
    singingCardLayout->addWidget(defaultSingingLanguageItem);
    singingCardLayout->addWidget(new DividerLine(Qt::Horizontal));
    singingCardLayout->addWidget(defaultLyricItem);
    singingCardLayout->setContentsMargins(10, 5, 10, 5);
    singingCardLayout->setSpacing(0);

    auto singingCard = new OptionsCard;
    singingCard->setTitle(tr("Singing"));
    singingCard->card()->setLayout(singingCardLayout);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configFileCard);
    mainLayout->addWidget(singingCard);
    mainLayout->addSpacerItem(
        new QSpacerItem(8, 4, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}
void GeneralPage::modifyOption() {
    auto option = appOptions->general();
    option->defaultSingingLanguage =
        static_cast<AppGlobal::LanguageType>(m_cbDefaultSingingLanguage->currentIndex());
    option->defaultLyric = m_leDefaultLyric->text();
    appOptions->saveAndNotify();
}