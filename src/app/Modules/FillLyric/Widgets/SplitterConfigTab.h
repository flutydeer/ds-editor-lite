#ifndef SPLITTER_CONFIG_TAB_H
#define SPLITTER_CONFIG_TAB_H

#include <QWidget>

#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"

class QPushButton;

namespace FillLyric
{
    class RuleListPanel;
    class SplitterDetailPanel;

    class SplitterConfigTab final : public QWidget {
        Q_OBJECT

    public:
        explicit SplitterConfigTab(QWidget *parent = nullptr);

        void loadFromOption(FillLyricOption *opt);

    Q_SIGNALS:
        void configChanged();
        void jumpToTestRequested();

    private:
        void onSelectionChanged(int currentRow);
        void onAddRule();
        void onRemoveRule();
        void onOrderChanged();
        void applyConfig();
        void saveCurrentDetail();
        void rebuildItemWidgets();

        RuleListPanel *m_listPanel = nullptr;
        SplitterDetailPanel *m_detailPanel = nullptr;
        QPushButton *m_applyBtn = nullptr;

        struct RuleItem {
            QString name;
            bool builtin = false;
            bool enabled = true;
            SplitterRuleInfo builtinInfo;
            CustomSplitterRule customRule;
        };
        QList<RuleItem> m_rules;
        int m_currentIndex = -1;
    };
} // namespace FillLyric

#endif // SPLITTER_CONFIG_TAB_H
