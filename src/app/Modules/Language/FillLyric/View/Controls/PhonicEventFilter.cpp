#include "PhonicEventFilter.h"

namespace FillLyric {
    PhonicEventFilter::PhonicEventFilter(QTableView *tableView, PhonicModel *model, QObject *parent)
        : QObject(parent), m_tableView(tableView), m_model(model) {
    }

    bool PhonicEventFilter::eventFilter(QObject *obj, QEvent *event) {
        if (event->type() == QEvent::KeyPress) {
            const auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                const auto index = m_tableView->currentIndex();
                if (!m_tableView->isPersistentEditorOpen(m_tableView->currentIndex()) &&
                    m_model->cellLanguage(index.row(), index.column()) != "Slur") {
                    Q_EMIT this->lineBreak(index);
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Delete) {
                const auto selected = m_tableView->selectionModel()->selectedIndexes();
                Q_EMIT this->cellClear(selected);
                return true;
            }
        } else if (event->type() == QEvent::Wheel) {
            auto *wheelEvent = dynamic_cast<QWheelEvent *>(event);
            if (wheelEvent->modifiers() == Qt::ControlModifier) {
                const int fontSizeDelta = wheelEvent->angleDelta().y() / 120;
                QFont font = m_tableView->font();
                font.setPointSize(font.pointSize() + fontSizeDelta);
                m_tableView->setFont(font);

                wheelEvent->accept();
                Q_EMIT fontSizeChanged();
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
} // FillLyric