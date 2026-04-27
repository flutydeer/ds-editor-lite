#ifndef LYRIC_TAB_LYRIC_TAB_H
#define LYRIC_TAB_LYRIC_TAB_H

#include "Modules/FillLyric/LangCommon.h"
#include "Modules/FillLyric/LyricTabConfig.h"

#include "Modules/FillLyric/Widgets/LyricBaseWidget.h"
#include "Modules/FillLyric/Widgets/LyricExtWidget.h"

namespace FillLyric
{
    class LyricTab final : public QWidget {
        Q_OBJECT

    public:
        explicit LyricTab(const QList<LangNote> &langNotes, const QStringList &priorityG2pIds = {},
                          QMap<QString, QString> langToG2pId = {}, const LyricTabConfig &config = {},
                          QWidget *parent = nullptr);
        ~LyricTab() override;

        void setLangNotes(bool warn = true);

        QList<QList<LangNote>> exportLangNotes() const;
        QList<QList<LangNote>> modelExport() const;

        bool exportSkipSlur() const;

    Q_SIGNALS:
        void shrinkWindowRight(int newWidth);
        void expandWindowRight();
        void modifyOptionSignal(const FillLyric::LyricTabConfig &config);

    public Q_SLOTS:
        void onBtnInsertTextClicked() const;
        void onBtnToTableClicked() const;

    private:
        void modifyOption();

        std::vector<std::string> m_priorityG2pIds;
        QMap<std::string, std::string> m_langToG2pId;

        QList<LangNote *> m_langNotes;

        LyricBaseWidget *m_lyricBaseWidget;
        LyricExtWidget *m_lyricExtWidget;

        int m_notesCount = 0;
        bool m_exportLanguage = false;

        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_lyricLayout;
    };

} // namespace FillLyric

#endif // LYRIC_TAB_LYRIC_TAB_H
