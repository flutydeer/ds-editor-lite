#ifndef RULE_LIST_WIDGET_H
#define RULE_LIST_WIDGET_H

#include <QListWidget>

namespace FillLyric
{
    class RuleListWidget final : public QListWidget {
        Q_OBJECT

    public:
        explicit RuleListWidget(QWidget *parent = nullptr);

    Q_SIGNALS:
        void orderChanged();

    protected:
        void dropEvent(QDropEvent *event) override;
    };
} // namespace FillLyric

#endif // RULE_LIST_WIDGET_H
