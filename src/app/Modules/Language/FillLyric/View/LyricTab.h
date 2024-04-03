#ifndef DS_EDITOR_LITE_LYRICWIDGET_H
#define DS_EDITOR_LITE_LYRICWIDGET_H

#include <QWidget>

#include "Widgets/LyricBaseWidget.h"
#include "Widgets/LyricExtWidget.h"

class LineEdit;

namespace FillLyric {
    class LyricTab final : public QWidget {
        Q_OBJECT
        friend class LyricDialog;

    public:
        explicit LyricTab(QList<LangNote *> langNotes, QWidget *parent = nullptr);
        ~LyricTab() override;

        LyricBaseWidget *m_lyricBaseWidget;
        LyricExtWidget *m_lyricExtWidget;

        void setLangNotes();

        [[nodiscard]] QList<QList<LangNote>> exportLangNotes() const;
        [[nodiscard]] QList<QList<LangNote>> modelExport() const;

        [[nodiscard]] bool exportSkipSlur() const;
        [[nodiscard]] bool exportLanguage() const;

    Q_SIGNALS:
        void shrinkWindowRight(int newWidth);
        void expandWindowRight();

    public Q_SLOTS:
        void _on_btnInsertText_clicked() const;
        void _on_btnToTable_clicked() const;

    private:
        void modifyOption() const;

        QList<LangNote *> m_langNotes;

        // Variables
        int notesCount = 0;

        // Layout
        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_lyricLayout;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_LYRICWIDGET_H
