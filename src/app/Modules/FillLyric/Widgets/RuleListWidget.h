#ifndef RULE_LIST_WIDGET_H
#define RULE_LIST_WIDGET_H

#include <QListWidget>

class ItemViewReorderController;

namespace FillLyric {
    class RuleListWidget final : public QListWidget {
        Q_OBJECT

    public:
        explicit RuleListWidget(QWidget *parent = nullptr);

    Q_SIGNALS:
        void orderChanged();

    protected:
        void dropEvent(QDropEvent *event) override;
        void startDrag(Qt::DropActions supportedActions) override;

    private:
        ItemViewReorderController *m_reorder = nullptr;
    };
} // namespace FillLyric

#endif // RULE_LIST_WIDGET_H
