#ifndef TAGGER_CONFIG_TAB_H
#define TAGGER_CONFIG_TAB_H

#include <QWidget>

#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

class QPushButton;

namespace FillLyric
{
    class RuleListPanel;
    class TaggerDetailPanel;

    class TaggerConfigTab final : public QWidget {
        Q_OBJECT

    public:
        explicit TaggerConfigTab(QWidget *parent = nullptr);

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
        TaggerDetailPanel *m_detailPanel = nullptr;
        QPushButton *m_applyBtn = nullptr;

        struct RuleItem {
            QString name;
            bool builtin = false;
            bool enabled = true;
            TaggerRuleInfo builtinInfo;
            CustomTaggerRule customRule;
        };
        QList<RuleItem> m_rules;
        int m_currentIndex = -1;
        QStringList m_knownLanguages;
    };
} // namespace FillLyric

#endif // TAGGER_CONFIG_TAB_H
