#ifndef LYRICWRAPWIDGET_H
#define LYRICWRAPWIDGET_H

#include <QScrollArea>
#include "LyricWrapView.h"

namespace LyricWrap {

    class LyricWrapWidget final : public QScrollArea {
        Q_OBJECT
    public:
        explicit LyricWrapWidget(QWidget *parent = nullptr);

        void appendList(const QList<LangNote *> &noteList) const;

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        LyricWrapView *m_view;
    };

} // LyricWrap

#endif // LYRICWRAPWIDGET_H
