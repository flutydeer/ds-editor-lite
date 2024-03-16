#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QTabWidget>

#include <Model/Note.h>

#include "Modules/Language/FillLyric/View/Tabs/LyricTab.h"
#include "Modules/Language/LangMgr/Widgets/LangMgrWidget.h"

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Controls/Button.h"

class LyricDialog final : public Dialog {
    Q_OBJECT
public:
    explicit LyricDialog(QList<Note *> note, QWidget *parent = nullptr);
    ~LyricDialog() override;

    void exportPhonics();

private:
    void noteToPhonic();

    void shrinkWindowRight(const int &newWidth);
    void expandWindowRight();

    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;

    FillLyric::LyricTab *m_lyricWidget;
    LangMgr::LangMgrWidget *m_langMgrWidget;

    Button *m_btnOk;
    Button *m_btnCancel;

    QList<Note *> m_notes;
    QList<FillLyric::Phonic *> m_phonics;
};

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
