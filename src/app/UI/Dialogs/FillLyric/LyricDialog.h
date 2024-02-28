#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QTabWidget>

#include <Model/Note.h>

#include "View/LyricWidget.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Controls/Button.h"

namespace FillLyric {

    class LyricDialog final: public Dialog {
        Q_OBJECT
    public:
        explicit LyricDialog(QList<Note *> note, QWidget *parent = nullptr);
        ~LyricDialog() override;

        void exportPhonics();

    private:
        void noteToPhonic();

        void shrinkWindowRight(int newWidth);
        void expandWindowRight();

        QVBoxLayout *m_mainLayout;
        QTabWidget *m_tabWidget;

        LyricWidget *m_lyricWidget;

        Button *m_btnOk;
        Button *m_btnCancel;

        QList<Note *> m_notes;
        QList<Phonic *> m_phonics;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
