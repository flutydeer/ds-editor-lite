#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QDialog>
#include <QTabWidget>

#include <Model/Note.h>

#include "View/LyricWidget.h"
#include "Window/Dialogs/Dialog.h"
#include "Controls/Button.h"

namespace FillLyric {

    class LyricDialog : public Dialog {
        Q_OBJECT
    public:
        explicit LyricDialog(QList<Note *> note, QWidget *parent = nullptr);
        ~LyricDialog() override;

    private:
        void noteToPhonic();
        void phonicToNote();

        QVBoxLayout *m_mainLayout;
        QTabWidget *m_tabWidget;

        LyricWidget *m_lyricWidget;

        Button *m_btnOk;
        Button *m_btnCancel;

        QList<Note *> m_notes;
        QList<PhonicNote *> m_phonicNotes;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
