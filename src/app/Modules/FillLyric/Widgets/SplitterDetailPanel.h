#ifndef SPLITTER_DETAIL_PANEL_H
#define SPLITTER_DETAIL_PANEL_H

#include <QWidget>

#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/TextSplitter.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;

namespace FillLyric
{
    class SplitterDetailPanel final : public QWidget {
        Q_OBJECT

    public:
        explicit SplitterDetailPanel(QWidget *parent = nullptr);

        // Show placeholder (no selection)
        void showPlaceholder();
        // Show readonly view for a builtin rule
        void showBuiltinRule(const SplitterRuleInfo &info);
        // Show editable view for a custom rule
        void showCustomRule(const CustomSplitterRule &rule);

        // Collect current edits into a CustomSplitterRule
        [[nodiscard]] CustomSplitterRule collectCustomRule() const;

    private:
        QStackedWidget *m_stack = nullptr;

        // Page 0: placeholder
        QLabel *m_placeholderLabel = nullptr;

        // Page 1: builtin readonly
        QLabel *m_builtinNameLabel = nullptr;
        QPlainTextEdit *m_builtinRegexView = nullptr;

        // Page 2: custom editable
        QLineEdit *m_nameEdit = nullptr;
        QPlainTextEdit *m_regexEdit = nullptr;
        QLabel *m_infoLabel = nullptr;
    };
} // namespace FillLyric

#endif // SPLITTER_DETAIL_PANEL_H
