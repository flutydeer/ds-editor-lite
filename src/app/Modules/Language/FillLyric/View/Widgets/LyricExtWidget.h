#ifndef LYRICEXTWIDGET_H
#define LYRICEXTWIDGET_H

#include <QWidget>

#include <QCheckBox>

#include "TableConfigWidget.h"

#include "../Controls/LyricWrapView.h"

#include "UI/Controls/Button.h"
#include "UI/Controls/SwitchButton.h"
#include "UI/Controls/OptionsCardItem.h"

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

        QUndoStack *m_history;

        int *notesCount = nullptr;
        QHBoxLayout *m_tableTopLayout;

        QHBoxLayout *m_mainLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_tableCountLayout;
        QHBoxLayout *m_epOptLabelLayout;

        QWidget *m_epOptWidget;
        QVBoxLayout *m_epOptLayout;

        // Widgets
        LyricWrapView *m_wrapView;
        TableConfigWidget *m_tableConfigWidget;

        Button *m_btnToText;

        // Labels
        QLabel *noteCountLabel;

        Button *btnFoldLeft;
        Button *btnToggleFermata;
        OptionsCardItem *autoWrapItem;
        SwitchButton *autoWrap;
        QPushButton *btnUndo;
        QPushButton *btnRedo;
        Button *m_btnInsertText;
        QPushButton *btnTableConfig;

        QLabel *exportOptLabel;
        QPushButton *exportOptButton;

        QCheckBox *exportSkipSlur;
        QCheckBox *exportLanguage;
    };

} // FillLyric

#endif // LYRICEXTWIDGET_H
