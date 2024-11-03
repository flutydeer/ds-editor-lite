#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QTabWidget>

#include <lyric-tab/LyricTab.h>
#include <language-manager/LangCommon.h>

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Options/Pages/G2pPage.h"

class LanguagePage;
class Note;
class AccentButton;

struct LyricResult {
    QList<LangNote> langNotes;
    bool skipSlur = false;
    bool exportLang = false;
};

class LyricDialog final : public Dialog {
    Q_OBJECT

public:
    explicit LyricDialog(QList<Note *> note, const QStringList &priorityG2pIds = {},
                         QWidget *parent = nullptr);
    ~LyricDialog() override;

    void setLangNotes() const;

    LyricResult noteResult() const;
    LyricResult exportLangNotes() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void accept() override;

private:
    void noteToPhonic();

    void shrinkWindowRight(const int &newWidth);
    void expandWindowRight();

    void switchTab(const int &index);

    static void _on_modifyOption(const FillLyric::LyricTabConfig &config);

    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;

    FillLyric::LyricTab *m_lyricWidget;
    G2pPage *m_g2pPage;

    AccentButton *m_btnOk;
    Button *m_btnCancel;

    QList<Note *> m_notes;
    QList<LangNote> m_langNotes;
    LyricResult m_noteResult;
};

#endif // DS_EDITOR_LITE_LYRICDIALOG_H