#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <lite/ProjectModel/AppModel/SingingClip.h>

#include "Modules/FillLyric/LyricTab.h"
#include "Modules/FillLyric/LangCommon.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include "UI/Dialogs/Base/Dialog.h"

// #include "UI/Dialogs/Options/Pages/G2pPage.h"

namespace FillLyric {
    class SplitterConfigTab;
    class TaggerConfigTab;
    class RuleTestTab;
}

class LanguagePage;
class Note;
class AccentButton;

struct LyricResult {
    QList<LangNote> langNotes;
    bool skipSlur = false;
};

class LyricDialog final : public Dialog {
    Q_OBJECT

public:
    explicit LyricDialog(SingingClip *clip, QList<Note *> note, SingerIdentifier singer,
                         const QStringList &priorityLanguages = {}, QWidget *parent = nullptr);
    ~LyricDialog() override;

    void setLangNotes() const;

    LyricResult noteResult() const;
    LyricResult exportLangNotes() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void accept() override;

private:
    void noteToPhonic();
    void ensureTabInitialized(int index);

    void shrinkWindowRight(const int &newWidth);
    void expandWindowRight();

    static void _on_modifyOption(const FillLyric::LyricTabConfig &config);

    SingingClip *m_clip;

    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;

    FillLyric::LyricTab *m_lyricWidget;
    QWidget *m_splitterConfigPage = nullptr;
    QWidget *m_taggerConfigPage = nullptr;
    QWidget *m_ruleTestPage = nullptr;

    FillLyric::SplitterConfigTab *m_splitterConfigTab = nullptr;
    FillLyric::TaggerConfigTab *m_taggerConfigTab = nullptr;
    FillLyric::RuleTestTab *m_ruleTestTab = nullptr;
    // G2pPage *m_g2pPage;

    AccentButton *m_btnOk;
    Button *m_btnCancel;

    QList<Note *> m_notes;
    QList<LangNote> m_langNotes;
    LyricResult m_noteResult;
};

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
