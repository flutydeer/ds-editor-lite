#ifndef DS_EDITOR_LITE_LYRICWIDGET_H
#define DS_EDITOR_LITE_LYRICWIDGET_H

#include <QObject>
#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "PhonicTextEdit.h"
#include "PhonicWidget.h"

namespace FillLyric {

    class LyricWidget : public QWidget {
        Q_OBJECT
    public:
        explicit LyricWidget(QList<PhonicNote *> phonicNotes, QWidget *parent = nullptr);
        ~LyricWidget() override;

    public Q_SLOTS:
        // Buttons
        void _on_btnInsertText_clicked();
        void _on_btnToTable_clicked();
        void _on_btnToText_clicked();
        void _on_btnImportLrc_clicked();

        // count
        void _on_textEditChanged();
        void _on_modelDataChanged();

    private:
        QList<PhonicNote *> m_phonicNotes;

        // Variables
        int notesCount = 0;

        // Layout
        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_tableTopLayout;
        QHBoxLayout *m_lyricLayout;
        QVBoxLayout *m_textEditLayout;
        QHBoxLayout *m_textTopLayout;
        QVBoxLayout *m_lyricOptLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_skipSlurLayout;
        QHBoxLayout *m_splitLayout;

        // Widgets
        PhonicTextEdit *m_textEdit;
        PhonicWidget *m_phonicWidget;

        // Labels
        QLabel *m_textCountLabel;
        QLabel *noteCountLabel;
        QLabel *splitLabel;

        // Buttons
        QPushButton *btnInsertText;
        QPushButton *btnToTable;
        QPushButton *btnToText;
        QPushButton *btnToggleFermata;
        QPushButton *btnImportLrc;

        QPushButton *btnUndo;
        QPushButton *btnRedo;

        // CheckBox
        QCheckBox *skipSlur;
        QCheckBox *splitBySpace;

        QComboBox *splitComboBox;
        QPushButton *btnSetting;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICWIDGET_H
