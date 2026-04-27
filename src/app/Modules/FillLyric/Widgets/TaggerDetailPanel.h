#ifndef TAGGER_DETAIL_PANEL_H
#define TAGGER_DETAIL_PANEL_H

#include <QWidget>

#include "Model/AppOptions/Options/FillLyricOption.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace FillLyric
{
    class TaggerDetailPanel final : public QWidget {
        Q_OBJECT

    public:
        explicit TaggerDetailPanel(QWidget *parent = nullptr);

        void showPlaceholder();
        void showBuiltinRule(const TaggerRuleInfo &info);
        void showCustomRule(const CustomTaggerRule &rule, const QStringList &knownLanguages);

        [[nodiscard]] CustomTaggerRule collectCustomRule() const;

        void setKnownLanguages(const QStringList &languages);

    private:
        void addCustomEntryUI(const CustomTaggerEntry &entry);
        void clearCustomEntries();

        QStackedWidget *m_stack = nullptr;
        QStringList m_knownLanguages;

        // Page 0: placeholder
        QLabel *m_placeholderLabel = nullptr;

        // Page 1: builtin readonly
        QLabel *m_builtinNameLabel = nullptr;
        QPlainTextEdit *m_builtinEntriesView = nullptr;

        // Page 2: custom editable
        QComboBox *m_langCombo = nullptr;
        QVBoxLayout *m_entriesLayout = nullptr;
        QPushButton *m_addEntryBtn = nullptr;

        struct EntryRowUI {
            QWidget *widget = nullptr;
            QComboBox *typeCombo = nullptr;
            QLineEdit *tagEdit = nullptr;
            QPlainTextEdit *valuesEdit = nullptr;
            QCheckBox *discardCheck = nullptr;
        };
        QList<EntryRowUI> m_entryRows;
    };
} // namespace FillLyric

#endif // TAGGER_DETAIL_PANEL_H
