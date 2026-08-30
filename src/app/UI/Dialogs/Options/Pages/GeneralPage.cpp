#include "GeneralPage.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/DirSelector.h>
#include <lite/GUI/Controls/FileSelector.h>
#include <lite/GUI/Controls/LineEdit.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/PathEditor.h>
#include "UI/Views/Common/LanguageComboBox.h"
#include "Global/AppOptionsGlobal.h"
#include "Utils/AppLogDirectory.h"
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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto snapshot = runtime->settings().getSettings();
    if (!snapshot)
        return;
    auto settings = snapshot.get().general;
    settings.uiLanguage = m_cbUiLanguage->currentData().toString();
    settings.defaultSingingLanguage = m_cbDefaultSingingLanguage->currentLanguage();
    m_defaultLyrics[settings.defaultSingingLanguage] = m_leDefaultLyric->text();
    settings.defaultLyrics = m_defaultLyrics;
    settings.gameDirectory = m_fsGameDir->path();
    settings.pitchModelPath = m_fsRmvpePath->path();
    settings.libreSvipPath = m_fsLibreSVIPPath->path();
    runtime->settings().updateGeneral({}, settings);
}

QWidget *GeneralPage::createContentWidget() {
    const auto widget = new QWidget;
    const auto option = appOptions->general();
    m_defaultLyrics = option->defaultLyrics;

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

    m_btnOpenLogFolder = new Button(tr("Open Folder..."), this);
    connect(m_btnOpenLogFolder, &Button::clicked, this,
            [] { AppLogDirectory::openLogDirectory(); });

    const auto appDataCard = new OptionListCard(tr("App Data"));
    appDataCard->addItem(tr("Config File"), m_btnOpenConfigFolder);
    appDataCard->addItem(tr("Log Folder"), m_btnOpenLogFolder);

    const auto langKey = option->defaultSingingLanguage;
    m_cbDefaultSingingLanguage = new LanguageComboBox(langKey);
    m_previousLanguage = langKey;
    connect(m_cbDefaultSingingLanguage, &ComboBox::currentIndexChanged, this, [this]() {
        m_defaultLyrics[m_previousLanguage] = m_leDefaultLyric->text();
        const auto newLang = m_cbDefaultSingingLanguage->currentLanguage();
        m_leDefaultLyric->setText(m_defaultLyrics.value(newLang, QStringLiteral("la")));
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
    connect(m_packageSearchPaths, &PathEditor::pathsChanged, this, [this]() {
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->settings().setPackageSearchPaths({}, m_packageSearchPaths->paths());
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
    m_fsLibreSVIPPath = new FileSelector;
    m_fsLibreSVIPPath->setMinimumWidth(480);
    m_fsLibreSVIPPath->setFilter(tr("Executable (*.exe)"));
    m_fsLibreSVIPPath->setPath(option->libreSVIPPath);
    connect(m_fsLibreSVIPPath, &FileSelector::pathChanged, this, &GeneralPage::modifyOption);

    const auto modelCard = new OptionListCard(tr("Model"));
    modelCard->addItem(tr("Game Model Dir"), m_fsGameDir);
    modelCard->addItem(tr("Rmvpe Model Path"), m_fsRmvpePath);
    modelCard->addItem(tr("LibreSVIP Path"), m_fsLibreSVIPPath);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(applicationCard);
    mainLayout->addWidget(appDataCard);
    mainLayout->addWidget(singingCard);
    mainLayout->addWidget(packagePathsCard);
    mainLayout->addWidget(modelCard);
    mainLayout->addStretch();
    mainLayout->setContentsMargins({});

    widget->setLayout(mainLayout);
    widget->setContentsMargins({});
    return widget;
}
