#ifndef RULE_LIST_PANEL_H
#define RULE_LIST_PANEL_H

#include <QWidget>

class QPushButton;

namespace FillLyric
{
    class RuleListWidget;

    class RuleListPanel final : public QWidget {
        Q_OBJECT

    public:
        explicit RuleListPanel(QWidget *parent = nullptr);

        RuleListWidget *listWidget() const;

        void setRemoveEnabled(bool enabled);

    Q_SIGNALS:
        void addRequested();
        void removeRequested();

    private:
        RuleListWidget *m_listWidget = nullptr;
        QPushButton *m_addBtn = nullptr;
        QPushButton *m_removeBtn = nullptr;
    };
} // namespace FillLyric

#endif // RULE_LIST_PANEL_H
