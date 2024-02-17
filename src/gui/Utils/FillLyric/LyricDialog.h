#ifndef DS_EDITOR_LITE_LYRICDIALOG_H
#define DS_EDITOR_LITE_LYRICDIALOG_H

#include <QDialog>

#include "PhonicWidget.h"

namespace FillLyric {

    class LyricDialog : public QDialog {
        Q_OBJECT
    public:
        explicit LyricDialog(QWidget *parent = nullptr);
        ~LyricDialog() override;

        void setLyrics(QList<QList<QString>> &lyrics) {
            m_phonicWidget->setLyrics(lyrics);
        }

        QList<Phonic> exportPhonics() {
            return m_phonicWidget->exportPhonics();
        }

    private:
        PhonicWidget *m_phonicWidget;
        QPushButton *m_btnOk;
        QPushButton *m_btnCancel;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
