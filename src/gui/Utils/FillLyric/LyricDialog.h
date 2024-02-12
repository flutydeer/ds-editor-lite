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

        void setLyrics(const QString &lyric) {
            m_phonicWidget->setLyrics(lyric);
        }

    private:
        PhonicWidget *m_phonicWidget;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICDIALOG_H
