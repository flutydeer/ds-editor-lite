#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QTabWidget>

#include "Modules/Language/FillLyric/View/LyricTab.h"
#include "UI/Dialogs/Base/Dialog.h"

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
    explicit LyricDialog(QList<Note *> note, QWidget *parent = nullptr);
    ~LyricDialog() override;

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

    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;

    FillLyric::LyricTab *m_lyricWidget;
    LanguagePage *m_langPage;

    AccentButton *m_btnOk;
    Button *m_btnCancel;

    QList<Note *> m_notes;
    QList<LangNote> m_langNotes;
    LyricResult m_noteResult;
};

#endif // DS_EDITOR_LITE_LYRICDIALOG_H