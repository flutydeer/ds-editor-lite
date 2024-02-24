#ifndef DS_EDITOR_LITE_PHONICEVENTFILTER_H
#define DS_EDITOR_LITE_PHONICEVENTFILTER_H

#include <QObject>
#include <QWheelEvent>

#include "../Model/PhonicModel.h"

namespace FillLyric {
    class PhonicEventFilter final: public QObject {
        Q_OBJECT
    public:
        PhonicEventFilter(QTableView *tableView, PhonicModel *model, QObject *parent = nullptr);

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override;

    private:
        QTableView *m_tableView;
        PhonicModel *m_model;

    Q_SIGNALS:
        void fontSizeChanged();
        void lineBreak(QModelIndex index);
        void cellClear(QList<QModelIndex> indexes);
    };
}
#endif // DS_EDITOR_LITE_PHONICEVENTFILTER_H
