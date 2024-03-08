#ifndef LYRICBASEWIDGET_H
#define LYRICBASEWIDGET_H

#include <QWidget>

namespace FillLyric {

    class LyricBaseWidget final : public QWidget {
        Q_OBJECT
        friend class LyricTab;
    public:
        explicit LyricBaseWidget(QWidget *parent = nullptr);
        ~LyricBaseWidget() override;
    };


} // FillLyric

#endif // LYRICBASEWIDGET_H
