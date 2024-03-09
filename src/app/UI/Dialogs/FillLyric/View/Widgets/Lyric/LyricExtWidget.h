#ifndef LYRICEXTWIDGET_H
#define LYRICEXTWIDGET_H

#include <QWidget>

#include <QCheckBox>

#include "TableConfigWidget.h"

#include "../../Controls/PhonicWidget.h"

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
        int *notesCount = nullptr;
        QHBoxLayout *m_tableTopLayout;

        QHBoxLayout *m_mainLayout;
        QVBoxLayout *m_tableLayout;
        QHBoxLayout *m_tableCountLayout;
        QHBoxLayout *m_tableBottomLayout;

        // Widgets
        PhonicWidget *m_phonicWidget;
        TableConfigWidget *m_tableConfigWidget;

        // Labels
        QLabel *noteCountLabel;

        Button *btnFoldLeft;
        Button *btnToggleFermata;
        QCheckBox *autoWrap;
        QPushButton *btnUndo;
        QPushButton *btnRedo;
        QPushButton *btnTableConfig;

        QLabel *exportLabel;
        QCheckBox *exportSkipSlur;
        QCheckBox *exportExcludeSpace;
    };

} // FillLyric

#endif // LYRICEXTWIDGET_H
