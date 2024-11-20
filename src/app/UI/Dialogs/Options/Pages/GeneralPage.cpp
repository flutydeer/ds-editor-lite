//
// Created by fluty on 24-3-18.
//

#include "GeneralPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/FileSelector.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/OptionsCard.h"
#include "UI/Controls/OptionsCardItem.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QMCore/qmsystem.h>

// #ifdef Q_OS_WIN
// #  include <Windows.h>
// #  include <WinUser.h>
// #  include <shellapi.h>
// #endif

GeneralPage::GeneralPage(QWidget *parent) : IOptionPage(parent) {
    auto option = appOptions->general();

    m_btnOpenConfigFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenConfigFolder, &Button::clicked, this, [=] {
        QM::reveal(appOptions->configPath());
        // #ifdef Q_OS_WIN
        //         auto path = QDir::toNativeSeparators(appOptions->configPath());
        //         ShellExecuteW(NULL, L"open", L"explorer",
        //                       QString("/select, \"%1\"").arg(path).toStdWString().c_str(), NULL,
        //                       SW_SHOW);
        // #else
        //         auto folder = QFileInfo(appOptions->configPath()).absoluteDir().path();
        //         QDesktopServices::openUrl(QUrl(folder));
        // #endif
    });

    const auto configFileCard = new OptionListCard(tr("App Config"));
    configFileCard->addItem(tr("Config File"), m_btnOpenConfigFolder);
    configFileCard->setTitle(tr("App Config"));

    const auto langKey = option->defaultSingingLanguage;
    m_cbDefaultSingingLanguage = new LanguageComboBox(langKey);
    connect(m_cbDefaultSingingLanguage, &ComboBox::currentIndexChanged, this,
            &GeneralPage::modifyOption);

    m_leDefaultLyric = new LineEdit;
    m_leDefaultLyric->setFixedWidth(80);
    m_leDefaultLyric->setText(option->defaultLyric);
    connect(m_leDefaultLyric, &LineEdit::editingFinished, this, &GeneralPage::modifyOption);
    // m_leDefaultLyric->setPlaceholderText(option->defaultLyric);

    m_leDefaultSinger = new LineEdit;
    m_leDefaultSinger->setText(option->defaultSinger);
    m_leDefaultSinger->setMinimumWidth(480);
    connect(m_leDefaultSinger, &LineEdit::editingFinished, this, &GeneralPage::modifyOption);

    const auto singingCard = new OptionListCard(tr("Singing"));
    singingCard->addItem(tr("Default Singing Language"), m_cbDefaultSingingLanguage);
    singingCard->addItem(tr("Default Lyric"), m_leDefaultLyric);
    singingCard->addItem(tr("Default Singer"), m_leDefaultSinger);

    m_somePath = new FileSelector;
    m_somePath->setMinimumWidth(480);
    connect(m_somePath, &FileSelector::filePathChanged, this, &GeneralPage::modifyOption);
    m_rmvpePath = new FileSelector;
    m_rmvpePath->setMinimumWidth(480);
    connect(m_rmvpePath, &FileSelector::filePathChanged, this, &GeneralPage::modifyOption);

    const auto modelCard = new OptionListCard(tr("Model"));
    modelCard->addItem(tr("Some Model Path"), m_somePath);
    modelCard->addItem(tr("Rmvpe Model Path"), m_rmvpePath);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configFileCard);
    mainLayout->addWidget(singingCard);
    mainLayout->addWidget(modelCard);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
}

void GeneralPage::modifyOption() {
    const auto option = appOptions->general();
    option->defaultSingingLanguage = m_cbDefaultSingingLanguage->currentText();
    option->defaultLyric = m_leDefaultLyric->text();
    option->defaultSinger = m_leDefaultSinger->text();

    option->somePath = m_somePath->filePath();
    option->rmvpePath = m_rmvpePath->filePath();
    appOptions->saveAndNotify();
}