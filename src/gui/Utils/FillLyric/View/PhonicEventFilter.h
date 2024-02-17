#ifndef DS_EDITOR_LITE_PHONICEVENTFILTER_H
#define DS_EDITOR_LITE_PHONICEVENTFILTER_H

#include <QObject>
#include <QTableView>
#include <QKeyEvent>
#include <QWheelEvent>

#include "../Model/PhonicModel.h"
#include "PhonicWidget.h"

namespace FillLyric {
    class PhonicWidget;
    class PhonicEventFilter : public QObject {
        Q_OBJECT
    public:
        PhonicEventFilter(QTableView *tableView, PhonicModel *model, QObject *parent = nullptr)
            : QObject(parent), m_tableView(tableView), m_model(model) {
        }

    protected:
        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() == QEvent::KeyPress) {
                auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    auto index = m_tableView->currentIndex();
                    if (!m_tableView->isPersistentEditorOpen(m_tableView->currentIndex()) &&
                        m_model->cellLyricType(index.row(), index.column()) != LyricType::Fermata) {
                        emit lineBreak(index);
                        return true;
                    }
                } else if (keyEvent->key() == Qt::Key_Delete) {
                    auto selected = m_tableView->selectionModel()->selectedIndexes();
                    emit cellClear(selected);
                    return true;
                }
            } else if (event->type() == QEvent::Wheel) {
                auto *wheelEvent = dynamic_cast<QWheelEvent *>(event);
                if (wheelEvent->modifiers() == Qt::ControlModifier) {
                    int fontSizeDelta = wheelEvent->angleDelta().y() / 120;
                    QFont font = m_tableView->font();
                    font.setPointSize(font.pointSize() + fontSizeDelta);
                    m_tableView->setFont(font);

                    wheelEvent->accept();
                    emit fontSizeChanged();
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QTableView *m_tableView;
        PhonicModel *m_model;

    signals:
        void fontSizeChanged();
        void cellClear(QList<QModelIndex> indexes);
        void lineBreak(const QModelIndex &index);
    };
}
#endif // DS_EDITOR_LITE_PHONICEVENTFILTER_H
