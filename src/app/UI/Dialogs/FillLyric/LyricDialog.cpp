#include "LyricDialog.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include <QApplication>

#include <lite/ProjectModel/AppModel/Note.h>
#include "Model/AppOptions/AppOptions.h"
#include "Modules/FillLyric/Widgets/SplitterConfigTab.h"
#include "Modules/FillLyric/Widgets/TaggerConfigTab.h"
#include "Modules/FillLyric/Widgets/RuleTestTab.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/GUI/Controls/AccentButton.h>
// #include "UI/Dialogs/Options/Pages/G2pPage.h"

#include <QKeyEvent>
#include <QScreen>
#include <QStyle>

namespace {
    constexpr int kLyricTabIndex = 0;
    constexpr int kSplitterTabIndex = 1;
    constexpr int kTaggerTabIndex = 2;
    constexpr int kRuleTestTabIndex = 3;
    constexpr int kLyricBaseStretch = 1;
    constexpr int kLyricPreviewStretch = 2;
    constexpr int kLyricCompactPadding = 20;
    constexpr int kMinimumLyricCompactWidth = 300;
}

LyricDialog::LyricDialog(SingingClip *clip, QList<Note *> note, SingerIdentifier singer,
                         const QStringList &priorityLanguages, QWidget *parent)
    : Dialog(parent), m_clip(clip), m_notes(std::move(note)) {
    setModal(true);
    setMinimumSize(720, 450);
    setWindowTitle(tr("Fill Lyric"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    const auto size = QApplication::primaryScreen()->availableSize();
    resize(static_cast<int>(size.width() * 0.6), static_cast<int>(size.height() * 0.6));

    noteToPhonic();

    m_mainLayout = new QVBoxLayout();
    m_tabWidget = new QTabWidget();

    const bool lyricExtVisible = appOptions->fillLyric()->extVisible;
    m_lyricPreviewVisible = lyricExtVisible;

    m_lyricWidget = new FillLyric::LyricTab(
        m_langNotes, std::move(singer), SynthrtEngine::instance().languageService(),
        priorityLanguages,
        {appOptions->fillLyric()->baseVisible, lyricExtVisible,
         appOptions->fillLyric()->textEditFontSize, appOptions->fillLyric()->skipSlur,
         appOptions->fillLyric()->splitMode, appOptions->fillLyric()->viewFontSize,
         appOptions->fillLyric()->exportLanguage});

    m_lyricCompactWidth = lyricCompactWidthFor(width());
    if (!lyricExtVisible) {
        shrinkWindowRight(m_lyricCompactWidth);
    }

    // Apply saved Split/Tag config to engines
    FillLyric::TextSplitter::setBuiltinEnabled(appOptions->fillLyric()->builtinSplitterEnabled);
    FillLyric::TextSplitter::setCustomRules(appOptions->fillLyric()->customSplitterRules);
    FillLyric::TextSplitter::setRuleOrder(appOptions->fillLyric()->splitterOrder);
    FillLyric::TextTagger::setBuiltinEnabled(appOptions->fillLyric()->builtinTaggerEnabled);
    FillLyric::TextTagger::setCustomRules(appOptions->fillLyric()->customTaggerRules);
    FillLyric::TextTagger::setRuleOrder(appOptions->fillLyric()->taggerOrder);

    // m_g2pPage = new G2pPage(this);

    m_btnOk = new AccentButton(tr("&Import"), this);
    // m_btnOk->setPrimary(true);
    setPositiveButton(m_btnOk);
    m_btnCancel = new Button(tr("&Cancel"), this);
    setNegativeButton(m_btnCancel);

    m_splitterConfigPage = new QWidget(m_tabWidget);
    m_taggerConfigPage = new QWidget(m_tabWidget);
    m_ruleTestPage = new QWidget(m_tabWidget);

    m_tabWidget->addTab(m_lyricWidget, tr("Lyric"));
    m_tabWidget->addTab(m_splitterConfigPage, tr("Splitter"));
    m_tabWidget->addTab(m_taggerConfigPage, tr("Tagger"));
    m_tabWidget->addTab(m_ruleTestPage, tr("Test"));
    // m_tabWidget->addTab(m_g2pPage, tr("G2p"));
    m_tabWidget->addTab(new QWidget, tr("Help"));

    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->setContentsMargins({});
    body()->setLayout(m_mainLayout);

    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_lyricWidget, &FillLyric::LyricTab::shrinkWindowRight, this, [this] {
        m_lyricPreviewVisible = false;
        m_lyricCompactWidth = lyricCompactWidthFor(width());
        if (m_tabWidget->currentIndex() == kLyricTabIndex)
            shrinkWindowRight(m_lyricCompactWidth);
    });
    connect(m_lyricWidget, &FillLyric::LyricTab::expandWindowRight, this, [this] {
        m_lyricPreviewVisible = true;
        if (m_tabWidget->currentIndex() == kLyricTabIndex)
            expandWindowRight();
    });

    connect(m_lyricWidget, &FillLyric::LyricTab::modifyOptionSignal, this,
            &LyricDialog::_on_modifyOption);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &LyricDialog::onCurrentTabChanged);
}

LyricDialog::~LyricDialog() = default;

void LyricDialog::ensureTabInitialized(const int index) {
    if (index == kSplitterTabIndex && !m_splitterConfigTab) {
        auto *layout = new QVBoxLayout(m_splitterConfigPage);
        layout->setContentsMargins({});
        m_splitterConfigTab = new FillLyric::SplitterConfigTab(m_splitterConfigPage);
        m_splitterConfigTab->loadFromOption(appOptions->fillLyric());
        layout->addWidget(m_splitterConfigTab);

        connect(m_splitterConfigTab, &FillLyric::SplitterConfigTab::configChanged, m_lyricWidget,
                [this] { m_lyricWidget->setLangNotes(false); });
        connect(m_splitterConfigTab, &FillLyric::SplitterConfigTab::jumpToTestRequested, this,
                [this] { m_tabWidget->setCurrentIndex(kRuleTestTabIndex); });
    } else if (index == kTaggerTabIndex && !m_taggerConfigTab) {
        auto *layout = new QVBoxLayout(m_taggerConfigPage);
        layout->setContentsMargins({});
        m_taggerConfigTab = new FillLyric::TaggerConfigTab(m_taggerConfigPage);
        m_taggerConfigTab->loadFromOption(appOptions->fillLyric());
        layout->addWidget(m_taggerConfigTab);

        connect(m_taggerConfigTab, &FillLyric::TaggerConfigTab::configChanged, m_lyricWidget,
                [this] { m_lyricWidget->setLangNotes(false); });
        connect(m_taggerConfigTab, &FillLyric::TaggerConfigTab::jumpToTestRequested, this,
                [this] { m_tabWidget->setCurrentIndex(kRuleTestTabIndex); });
    } else if (index == kRuleTestTabIndex && !m_ruleTestTab) {
        auto *layout = new QVBoxLayout(m_ruleTestPage);
        layout->setContentsMargins({});
        m_ruleTestTab = new FillLyric::RuleTestTab(m_ruleTestPage);
        layout->addWidget(m_ruleTestTab);

        connect(m_ruleTestTab, &FillLyric::RuleTestTab::jumpToSplitterRequested, this,
                [this] { m_tabWidget->setCurrentIndex(kSplitterTabIndex); });
        connect(m_ruleTestTab, &FillLyric::RuleTestTab::jumpToTaggerRequested, this,
                [this] { m_tabWidget->setCurrentIndex(kTaggerTabIndex); });
    } else {
        return;
    }
}

void LyricDialog::onCurrentTabChanged(const int index) {
    if (index == kLyricTabIndex) {
        if (m_lyricPreviewVisible)
            expandWindowRight();
        else
            shrinkWindowRight(m_lyricCompactWidth);
    } else {
        expandWindowRight();
    }

    ensureTabInitialized(index);
}

int LyricDialog::lyricCompactWidthFor(const int expandedWidth) const {
    const auto bodyMargins = body()->contentsMargins();
    const int layoutSpacing =
        qMax(0, m_lyricWidget->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr,
                                                    m_lyricWidget));
    const int contentWidth =
        expandedWidth - bodyMargins.left() - bodyMargins.right() - layoutSpacing;
    const int totalStretch = kLyricBaseStretch + kLyricPreviewStretch;
    return qMax(kMinimumLyricCompactWidth,
                contentWidth * kLyricBaseStretch / totalStretch + kLyricCompactPadding);
}

void LyricDialog::setLangNotes() const {
    m_lyricWidget->setLangNotes(false);
}

void LyricDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        return;
    QDialog::keyPressEvent(event);
}

void LyricDialog::accept() {
    this->m_noteResult = exportLangNotes();
    Dialog::accept();
}

void LyricDialog::noteToPhonic() {
    const auto singerInfo = m_clip->singerInfo();
    for (const auto note : m_notes) {
        auto langNote = LangNote(note->lyric());
        langNote.syllable = note->pronunciation().original;
        langNote.syllableRevised = note->pronunciation().edited;
        langNote.candidates = note->pronCandidates();
        langNote.language = note->language();
        langNote.g2pId = singerInfo.g2pId(note->language());

        if (note->isSlur())
            langNote.g2pId = kSlurLyric;

        m_langNotes.append(langNote);
    }
}

void LyricDialog::shrinkWindowRight(const int &newWidth) {
    setMinimumSize(300, 450);
    resize(newWidth, height());
}

void LyricDialog::expandWindowRight() {
    setMinimumSize(720, 450);
    const auto size = QApplication::primaryScreen()->availableSize();
    resize(static_cast<int>(size.width() * 0.6), height());
}

LyricResult LyricDialog::noteResult() const {
    return m_noteResult;
}

LyricResult LyricDialog::exportLangNotes() const {
    const auto noteLists = m_lyricWidget->exportLangNotes();

    const bool skipSlurRes = m_lyricWidget->exportSkipSlur();

    QList<LangNote> result;
    for (const auto &langNotes : noteLists) {
        for (auto &langNote : langNotes) {
            result.append(langNote);
        }
    }
    return {result, skipSlurRes};
}

void LyricDialog::_on_modifyOption(const FillLyric::LyricTabConfig &config) {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    if (!runtime)
        return;
    const auto snapshot = runtime->settings().getSettings();
    if (!snapshot)
        return;
    auto settings = snapshot.get().fillLyric;
    settings.baseVisible = config.lyricBaseVisible;
    settings.extensionVisible = config.lyricExtVisible;
    settings.textEditFontSize = config.lyricBaseFontSize;
    settings.skipSlur = config.baseSkipSlur;
    settings.splitMode = config.splitMode;
    settings.viewFontSize = config.lyricExtFontSize;
    settings.exportLanguage = config.exportLanguage;
    runtime->settings().updateFillLyric({}, settings);
}
