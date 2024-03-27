#ifndef LYRICWRAPVIEW_H
#define LYRICWRAPVIEW_H

#include "LyricCell.h"
#include "CellList.h"
#include <QGraphicsView>

namespace LyricWrap {

    class LyricWrapView final : public QGraphicsView {
        Q_OBJECT
    public:
        explicit LyricWrapView(QWidget *parent = nullptr);
        ~LyricWrapView() override;

        void appendList(const QList<LangNote *> &noteList);

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        [[nodiscard]] qreal cellBaseY(const int &index) const;
        void repaintCellLists();
        void refreshSceneRect() const;

        QGraphicsScene *m_scene;

        QList<CellList *> m_cellLists;
        QList<qreal> m_heights;
    };


}
#endif // LYRICWRAPVIEW_H
