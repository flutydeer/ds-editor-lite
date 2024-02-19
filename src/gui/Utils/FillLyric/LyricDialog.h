#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QDialog>

#include <Model/Note.h>

#include "View/PhonicWidget.h"

namespace FillLyric {

    class LyricDialog : public QDialog {
        Q_OBJECT
    public:
        explicit LyricDialog(QList<Note *> note, QWidget *parent = nullptr);
        ~LyricDialog() override;

        QList<Phonic> exportPhonics();

    private:
        void noteToPhonic();
        void phonicToNote();

        PhonicWidget *m_phonicWidget;
        QPushButton *m_btnOk;
        QPushButton *m_btnCancel;

        QList<Note *> m_notes;
        QList<PhonicNote *> m_phonicNotes;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
