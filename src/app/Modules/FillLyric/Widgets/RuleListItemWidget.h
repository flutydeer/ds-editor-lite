#ifndef RULE_LIST_ITEM_WIDGET_H
#define RULE_LIST_ITEM_WIDGET_H

#include <QWidget>

class QCheckBox;
class QLabel;

namespace FillLyric
{
    class RuleListItemWidget final : public QWidget {
        Q_OBJECT

    public:
        explicit RuleListItemWidget(const QString &name, bool builtin, bool enabled,
                                    QWidget *parent = nullptr);

        [[nodiscard]] bool isRuleEnabled() const;
        void setRuleEnabled(bool enabled);
        [[nodiscard]] QString name() const;
        [[nodiscard]] bool isBuiltin() const;

    Q_SIGNALS:
        void enabledChanged(bool enabled);

    private:
        QLabel *m_handleLabel = nullptr;
        QCheckBox *m_checkbox = nullptr;
        QLabel *m_nameLabel = nullptr;
        QLabel *m_builtinLabel = nullptr;
        bool m_builtin = false;
    };
} // namespace FillLyric

#endif // RULE_LIST_ITEM_WIDGET_H
