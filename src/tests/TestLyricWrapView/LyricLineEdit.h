#ifndef LYRICLINEEDIT_H
#define LYRICLINEEDIT_H

#include <QLineEdit>
#include <QFocusEvent>

namespace LyricWrap {

    class LyricLineEdit final : public QLineEdit {
        Q_OBJECT
    public:
        explicit LyricLineEdit(QWidget *parent = nullptr);

    private Q_SLOTS:
        void adjustMaxWidth();
    };

} // LyricWarp

#endif // LYRICLINEEDIT_H
