#ifndef RULE_TEST_TAB_H
#define RULE_TEST_TAB_H

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace FillLyric
{
    class RuleTestTab final : public QWidget {
        Q_OBJECT

    public:
        explicit RuleTestTab(QWidget *parent = nullptr);

    Q_SIGNALS:
        void jumpToSplitterRequested();
        void jumpToTaggerRequested();

    private:
        void runTest();

        QPlainTextEdit *m_inputEdit = nullptr;
        QPushButton *m_runBtn = nullptr;
        QLabel *m_errorLabel = nullptr;

        // Split result
        QPlainTextEdit *m_splitOutput = nullptr;

        // Tag result
        QPlainTextEdit *m_tagOutput = nullptr;
    };
} // namespace FillLyric

#endif // RULE_TEST_TAB_H
