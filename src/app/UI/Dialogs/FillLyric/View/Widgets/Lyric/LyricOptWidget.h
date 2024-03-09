#ifndef LYRICOPTWIDGET_H
#define LYRICOPTWIDGET_H

#include <QWidget>
#include <QVBoxLayout>

#include <QLabel>

#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/LineEdit.h"

namespace FillLyric {

    class LyricOptWidget final : public QWidget {
        Q_OBJECT
        friend class LyricTab;

    public:
        explicit LyricOptWidget(QWidget *parent = nullptr);
        ~LyricOptWidget() override;

    private:
        QVBoxLayout *m_lyricOptLayout;

        QLabel *splitLabel;

        // Buttons
        Button *btnInsertText;
        Button *btnToTable;
        Button *btnToText;

        ComboBox *splitComboBox;
        Button *btnRegSetting;

        // LineEdit
        LineEdit *m_splitters;
    };

} // FillLyric

#endif // LYRICOPTWIDGET_H
