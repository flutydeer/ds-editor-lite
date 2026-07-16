//
// Created by fluty on 24-3-18.
//

#include "GeneralPage.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/CardView.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/DirSelector.h"
#include "UI/Controls/FileSelector.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/PathEditor.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "Global/AppOptionsGlobal.h"
#include "Utils/UiLanguageManager.h"

#include <QLabel>
#include <QListView>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QMCore/qmsystem.h>

GeneralPage::GeneralPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

void GeneralPage::modifyOption() {
    const auto option = appOptions->general();
    option->uiLanguage = m_cbUiLanguage->currentData().toString();
    option->defaultSingingLanguage = m_cbDefaultSingingLanguage->currentText();
    option->defaultLyrics[option->defaultSingingLanguage] = m_leDefaultLyric->text();

    option->gameDir = m_fsGameDir->path();
    option->rmvpePath = m_fsRmvpePath->path();
    appOptions->saveAndNotify(AppOptionsGlobal::Option::General);
}

QWidget *GeneralPage::createContentWidget() {
    const auto widget = new QWidget;
    const auto option = appOptions->general();

    m_cbUiLanguage = new ComboBox;
    m_cbUiLanguage->addItem(tr("Auto Detect"), UiLanguageManager::System);
    m_cbUiLanguage->addItem(QStringLiteral("English"), UiLanguageManager::English);
    m_cbUiLanguage->addItem(QStringLiteral("简体中文"), UiLanguageManager::SimplifiedChinese);
    const auto uiLanguageIndex = m_cbUiLanguage->findData(option->uiLanguage);
    m_cbUiLanguage->setCurrentIndex(uiLanguageIndex < 0 ? 0 : uiLanguageIndex);
    connect(m_cbUiLanguage, &ComboBox::currentIndexChanged, this, [this, option] {
        const auto previousLanguage = option->uiLanguage;
        modifyOption();
        if (previousLanguage == option->uiLanguage)
            return;
        if (const auto languageManager = UiLanguageManager::instance())
            languageManager->setPreference(option->uiLanguage);
    });

    const auto applicationCard = new OptionListCard(tr("Application"));
    applicationCard->addItem(tr("UI Language"), tr("Language used by the application interface"),
                             m_cbUiLanguage);

    m_btnOpenConfigFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenConfigFolder, &Button::clicked, this,
            [=] { QM::reveal(appOptions->configPath()); });

    const auto configFileCard = new OptionListCard(tr("App Config"));
    configFileCard->addItem(tr("Config File"), m_btnOpenConfigFolder);
    configFileCard->setTitle(tr("App Config"));

    const auto langKey = option->defaultSingingLanguage;
    m_cbDefaultSingingLanguage = new LanguageComboBox(langKey);
    m_previousLanguage = langKey;
    connect(m_cbDefaultSingingLanguage, &ComboBox::currentIndexChanged, this, [this]() {
        const auto option = appOptions->general();
        option->defaultLyrics[m_previousLanguage] = m_leDefaultLyric->text();
        const auto newLang = m_cbDefaultSingingLanguage->currentText();
        m_leDefaultLyric->setText(option->defaultLyricForLanguage(newLang));
        m_previousLanguage = newLang;
        modifyOption();
    });

    m_leDefaultLyric = new LineEdit;
    m_leDefaultLyric->setFixedWidth(80);
    m_leDefaultLyric->setText(option->defaultLyricForLanguage(langKey));
    connect(m_leDefaultLyric, &LineEdit::editingFinished, this, &GeneralPage::modifyOption);

    const auto singingCard = new OptionListCard(tr("Singing"));
    singingCard->addItem(tr("Default Singing Language"), m_cbDefaultSingingLanguage);
    singingCard->addItem(tr("Default Lyric"), m_leDefaultLyric);

    m_packageSearchPaths = new PathEditor;
    m_packageSearchPaths->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_packageSearchPaths->setPaths(option->packageSearchPaths);
    connect(m_packageSearchPaths, &PathEditor::pathsChanged, this, [option, this]() {
        option->setPackageSearchPathsAndNotify(m_packageSearchPaths->paths());
        appOptions->saveAndNotify(AppOptionsGlobal::Option::General);
    });

    auto packagePathsCard = new OptionsCard;
    const auto packagePathsLayout = new QHBoxLayout;
    packagePathsLayout->setContentsMargins(10, 10, 10, 10);
    packagePathsLayout->addWidget(m_packageSearchPaths, 0);
    packagePathsCard->card()->setLayout(packagePathsLayout);
    packagePathsCard->setTitle(tr("Package Search Paths (needs restart)"));
    packagePathsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QString onnxFilesFilter = tr("ONNX Files (*.onnx);;All Files (*)");

    m_fsGameDir = new FileSelector;
    m_fsGameDir->setMinimumWidth(480);
    m_fsGameDir->setFilter(tr("Directories"));
    m_fsGameDir->setDirMode(true);
    m_fsGameDir->setPath(option->gameDir);
    connect(m_fsGameDir, &FileSelector::pathChanged, this, &GeneralPage::modifyOption);
    m_fsRmvpePath = new FileSelector;
    m_fsRmvpePath->setMinimumWidth(480);
    m_fsRmvpePath->setFilter(onnxFilesFilter);
    m_fsRmvpePath->setFileDropExtensions({"onnx"});
    m_fsRmvpePath->setPath(option->rmvpePath);
    connect(m_fsRmvpePath, &FileSelector::pathChanged, this, &GeneralPage::modifyOption);

    const auto modelCard = new OptionListCard(tr("Model"));
    modelCard->addItem(tr("Game Model Dir"), m_fsGameDir);
    modelCard->addItem(tr("Rmvpe Model Path"), m_fsRmvpePath);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(applicationCard);
    mainLayout->addWidget(configFileCard);
    mainLayout->addWidget(singingCard);
    mainLayout->addWidget(packagePathsCard);
    mainLayout->addWidget(modelCard);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
