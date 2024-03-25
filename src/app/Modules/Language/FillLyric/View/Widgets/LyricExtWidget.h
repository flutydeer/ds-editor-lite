#ifndef LYRICEXTWIDGET_H
#define LYRICEXTWIDGET_H

#include <QWidget>

#include <QCheckBox>

#include "TableConfigWidget.h"

#include "../Controls/PhonicTableView.h"

#include "UI/Controls/Button.h"

namespace FillLyric {

    class LyricExtWidget final : public QWidget {
        Q_OBJECT
        friend class LyricTab;

    public:
        explicit LyricExtWidget(int *notesCount, QWidget *parent = nullptr);
        ~LyricExtWidget() override;

    public Q_SLOTS:
        // count
        void _on_modelDataChanged() const;

    private:
        void modifyOption() const;

        int *notesCount = nullptr;
        QHBoxLayout *m_tableTopLayout;

        QHBoxLayout *m_mainLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_tableCountLayout;
        QHBoxLayout *m_epOptLabelLayout;

        QWidget *m_epOptWidget;
        QVBoxLayout *m_epOptLayout;

        // Widgets
        PhonicTableView *m_phonicTableView;
        TableConfigWidget *m_tableConfigWidget;

        Button *m_btnToText;

        // Labels
        QLabel *noteCountLabel;

        Button *btnFoldLeft;
        Button *btnToggleFermata;
        QCheckBox *autoWrap;
        QPushButton *btnUndo;
        QPushButton *btnRedo;
        Button *m_btnInsertText;
        QPushButton *btnTableConfig;

        QLabel *exportOptLabel;
        QPushButton *exportOptButton;

        QCheckBox *exportSkipSlur;
        QCheckBox *exportSkipEndSpace;
        QCheckBox *exportLanguage;
    };

} // FillLyric

#endif // LYRICEXTWIDGET_H
