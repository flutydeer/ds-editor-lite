#ifndef DS_EDITOR_LITE_LYRICWIDGET_H
#define DS_EDITOR_LITE_LYRICWIDGET_H

#include <QWidget>

#include "../Widgets/Lyric/LyricBaseWidget.h"
#include "../Widgets/Lyric/LyricOptWidget.h"
#include "../Widgets/Lyric/LyricExtWidget.h"

#include "../../Model/PhonicCommon.h"

class LineEdit;

namespace FillLyric {
    class LyricTab final : public QWidget {
        Q_OBJECT
    public:
        explicit LyricTab(QList<Phonic *> phonics, QWidget *parent = nullptr);
        ~LyricTab() override;

        void setPhonics();

        [[nodiscard]] QList<Phonic> exportPhonics() const;
        [[nodiscard]] QList<Phonic> modelExport() const;

        [[nodiscard]] bool exportSkipSlur() const;
        [[nodiscard]] bool exportExcludeSpace() const;

    Q_SIGNALS:
        void shrinkWindowRight(int newWidth);
        void expandWindowRight();

    public Q_SLOTS:
        void _on_btnInsertText_clicked() const;
        void _on_btnToTable_clicked() const;
        void _on_btnToText_clicked() const;
        void _on_splitComboBox_currentIndexChanged(int index) const;

    private:
        QList<Phonic *> m_phonics;

        // Variables
        int notesCount = 0;

        // Layout
        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_lyricLayout;

        LyricBaseWidget *m_lyricBaseWidget;
        LyricOptWidget *m_lyricOptWidget;
        LyricExtWidget *m_lyricExtWidget;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICWIDGET_H
