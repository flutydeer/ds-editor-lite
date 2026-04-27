#include "LyricDialog.h"

#include <QApplication>

#include "Model/AppModel/Note.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/FillLyric/Widgets/SplitterConfigTab.h"
#include "Modules/FillLyric/Widgets/TaggerConfigTab.h"
#include "Modules/FillLyric/Widgets/RuleTestTab.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"
#include "UI/Controls/AccentButton.h"
// #include "UI/Dialogs/Options/Pages/G2pPage.h"

#include <QKeyEvent>
#include <QScreen>

LyricDialog::LyricDialog(SingingClip *clip, QList<Note *> note, const QStringList &priorityG2pIds,
                         const QMap<QString, QString> &langToG2pId, QWidget *parent)
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

    m_lyricWidget = new FillLyric::LyricTab(
        m_langNotes, priorityG2pIds, langToG2pId,
        {appOptions->fillLyric()->baseVisible, appOptions->fillLyric()->extVisible,
         appOptions->fillLyric()->textEditFontSize, appOptions->fillLyric()->skipSlur,
         appOptions->fillLyric()->splitMode, appOptions->fillLyric()->viewFontSize,
         appOptions->fillLyric()->exportLanguage});


    if (!appOptions->fillLyric()->extVisible) {
        shrinkWindowRight(300);
    }

    // Apply saved Split/Tag config to engines
    FillLyric::TextSplitter::setBuiltinEnabled(appOptions->fillLyric()->builtinSplitterEnabled);
    FillLyric::TextSplitter::setCustomRules(appOptions->fillLyric()->customSplitterRules);
    FillLyric::TextSplitter::setRuleOrder(appOptions->fillLyric()->splitterOrder);
    FillLyric::TextTagger::setBuiltinEnabled(appOptions->fillLyric()->builtinTaggerEnabled);
    FillLyric::TextTagger::setCustomRules(appOptions->fillLyric()->customTaggerRules);
    FillLyric::TextTagger::setRuleOrder(appOptions->fillLyric()->taggerOrder);

    // Splitter config tab
    m_splitterConfigTab = new FillLyric::SplitterConfigTab(this);
    m_splitterConfigTab->loadFromOption(appOptions->fillLyric());

    // Tagger config tab
    m_taggerConfigTab = new FillLyric::TaggerConfigTab(this);
    m_taggerConfigTab->loadFromOption(appOptions->fillLyric());

    // Rule test tab
    m_ruleTestTab = new FillLyric::RuleTestTab(this);

    // m_g2pPage = new G2pPage(this);

    m_btnOk = new AccentButton(tr("&Import"), this);
    // m_btnOk->setPrimary(true);
    setPositiveButton(m_btnOk);
    m_btnCancel = new Button(tr("&Cancel"), this);
    setNegativeButton(m_btnCancel);

    m_tabWidget->addTab(m_lyricWidget, tr("Lyric"));
    m_tabWidget->addTab(m_splitterConfigTab, tr("Splitter"));
    m_tabWidget->addTab(m_taggerConfigTab, tr("Tagger"));
    m_tabWidget->addTab(m_ruleTestTab, tr("Test"));
    // m_tabWidget->addTab(m_g2pPage, tr("G2p"));
    m_tabWidget->addTab(new QWidget, tr("Help"));

    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->setContentsMargins({});
    body()->setLayout(m_mainLayout);

    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_lyricWidget, &FillLyric::LyricTab::shrinkWindowRight, this,
            &LyricDialog::shrinkWindowRight);
    connect(m_lyricWidget, &FillLyric::LyricTab::expandWindowRight, this,
            &LyricDialog::expandWindowRight);

    connect(m_lyricWidget, &FillLyric::LyricTab::modifyOptionSignal, this,
            &LyricDialog::_on_modifyOption);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &LyricDialog::switchTab);

    connect(m_splitterConfigTab, &FillLyric::SplitterConfigTab::configChanged,
            m_lyricWidget, [this]() {
                // Re-split when splitter config changes
                m_lyricWidget->setLangNotes(false);
            });

    connect(m_taggerConfigTab, &FillLyric::TaggerConfigTab::configChanged,
            m_lyricWidget, [this]() {
                // Re-split when tagger config changes
                m_lyricWidget->setLangNotes(false);
            });

    // Jump between config tabs and test tab
    connect(m_splitterConfigTab, &FillLyric::SplitterConfigTab::jumpToTestRequested,
            this, [this]() { m_tabWidget->setCurrentWidget(m_ruleTestTab); });
    connect(m_taggerConfigTab, &FillLyric::TaggerConfigTab::jumpToTestRequested,
            this, [this]() { m_tabWidget->setCurrentWidget(m_ruleTestTab); });
    connect(m_ruleTestTab, &FillLyric::RuleTestTab::jumpToSplitterRequested,
            this, [this]() { m_tabWidget->setCurrentWidget(m_splitterConfigTab); });
    connect(m_ruleTestTab, &FillLyric::RuleTestTab::jumpToTaggerRequested,
            this, [this]() { m_tabWidget->setCurrentWidget(m_taggerConfigTab); });
}

LyricDialog::~LyricDialog() = default;

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
            langNote.g2pId = "slur";

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

void LyricDialog::switchTab(const int &index) {
    // if (index == 0) {
    //     if (!m_lyricWidget->m_lyricExtWidget->isVisible()) {
    //         this->shrinkWindowRight(300);
    //     }
    // } else {
    //     if (!m_lyricWidget->m_lyricExtWidget->isVisible()) {
    //         this->expandWindowRight();
    //     }
    // }
}

void LyricDialog::_on_modifyOption(const FillLyric::LyricTabConfig &config) {
    const auto options = appOptions->fillLyric();
    options->baseVisible = config.lyricBaseVisible;
    options->extVisible = config.lyricExtVisible;

    options->textEditFontSize = config.lyricBaseFontSize;
    options->skipSlur = config.baseSkipSlur;
    options->splitMode = config.splitMode;

    options->viewFontSize = config.lyricExtFontSize;
    options->exportLanguage = config.exportLanguage;
    appOptions->saveAndNotify(AppOptionsGlobal::FillLyric);
}