//
// Created by fluty on 24-3-16.
//

#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

class AudioPage;
class MidiPage;
class PseudoSingerPage;
class AppearancePage;
class LanguagePage;
class GeneralPage;
class InferencePage;
class IOptionPage;
class QListWidget;
class Button;
class QStackedWidget;

class AppOptionsDialog : public Dialog {
    Q_OBJECT

public:
    enum Page {
        General,
        Audio,
        Midi,
        PseudoSinger,
        Appearance,
        Language,
        Inference,
        PreviewFunctions,
        DeveloperOptions
    };

    explicit AppOptionsDialog(Page page, QWidget *parent = nullptr);
    ~AppOptionsDialog() override;

private slots:
    void onSelectionChanged(int index) const;

private:
    QStringList m_pageNames = {tr("General"),       tr("Audio"),      tr("MIDI"),
                               tr("Pseudo Singer"), tr("Appearance"), tr("Language"),
                               tr("Inference"),
                               /* tr("Preview Functions"),  tr("Developer Options")*/};

    QListWidget *m_tabList;
    QStackedWidget *m_PageContent;

    GeneralPage *m_generalPage;
    AudioPage *m_audioPage;
    MidiPage *m_midiPage;
    PseudoSingerPage *m_pseudoSingerPage;
    AppearancePage *m_appearancePage;
    LanguagePage *m_languagePage;
    InferencePage *m_inferencePage;
    QList<IOptionPage *> m_pages;
};



#endif // APPOPTIONSDIALOG_H
