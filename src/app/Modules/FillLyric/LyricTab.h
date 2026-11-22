#ifndef LYRIC_TAB_LYRIC_TAB_H
#define LYRIC_TAB_LYRIC_TAB_H

#include "Modules/FillLyric/LangCommon.h"
#include "Modules/FillLyric/LyricTabConfig.h"

#include "Modules/FillLyric/Widgets/LyricBaseWidget.h"
#include "Modules/FillLyric/Widgets/LyricExtWidget.h"
#include "Modules/FillLyric/Utils/G2pService.h"
#include "Modules/FillLyric/Utils/LyricLanguageContext.h"

namespace FillLyric {
    class LyricTab final : public QWidget {
        Q_OBJECT

    public:
        explicit LyricTab(const QList<LangNote> &langNotes, SingerIdentifier singer,
                          const srt::g2p::LanguageService &languageService,
                          LyricLanguageContextPtr languageContext,
                          const LyricTabConfig &config = {}, QWidget *parent = nullptr);
        ~LyricTab() override;

        void setLangNotes(bool warn = true);

        QList<QList<LangNote>> exportLangNotes() const;
        QList<QList<LangNote>> modelExport() const;

        bool exportSkipSlur() const;

    Q_SIGNALS:
        void shrinkWindowRight();
        void expandWindowRight();
        void modifyOptionSignal(const FillLyric::LyricTabConfig &config);
        void languageChoiceChanged();

    public Q_SLOTS:
        void onBtnInsertTextClicked();
        void onBtnToTableClicked();

    private:
        void ensureOriginalNotesG2p();
        void resetOriginalNotesG2p();
        void initializeOriginalNotesPreview();
        void modifyOption();
        void onLanguageChoiceChanged();
        [[nodiscard]] bool confirmPreviewRebuild() const;
        [[nodiscard]] bool previewHasRevisions() const;
        [[nodiscard]] const LyricLanguageContext &languageContext() const;

        LyricLanguageContextPtr m_languageContext;
        G2pService m_g2pService;

        QList<LangNote *> m_langNotes;
        // The language each note declared before the first conversion replaced it.
        QStringList m_declaredLanguages;

        LyricBaseWidget *m_lyricBaseWidget;
        LyricExtWidget *m_lyricExtWidget;

        int m_notesCount = 0;
        bool m_previewInitialized = false;
        bool m_originalNotesG2pReady = false;

        QVBoxLayout *m_mainLayout;
        QHBoxLayout *m_lyricLayout;
    };

} // namespace FillLyric

#endif // LYRIC_TAB_LYRIC_TAB_H
