//
// Created by fluty on 24-3-16.
//

#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"
#include "Global/AppOptionsGlobal.h"

class AudioPage;
class MidiPage;
class PseudoSingerPage;
class AppearancePage;
// class LanguagePage;
// class G2pPage;
class GeneralPage;
class InferencePage;
class DeveloperPage;
class IOptionPage;
class QListWidget;
class Button;
class QStackedWidget;

class AppOptionsDialog : public Dialog {
    Q_OBJECT

public:
    explicit AppOptionsDialog(AppOptionsGlobal::Option option, QWidget *parent = nullptr);
    ~AppOptionsDialog() override;

private slots:
    void onSelectionChanged(int index) const;

private:
    QStringList pageNames = {tr("General"),    tr("Audio"), tr("MIDI"),
                               tr("Appearance"), /* tr("Language"), */  tr("Inference"),
                               tr("Developer Options") /* tr("Preview Functions") */};

    QListWidget *tabList;
    QStackedWidget *pageContent;

    GeneralPage *generalPage;
    AudioPage *audioPage;
    MidiPage *midiPage;
    // PseudoSingerPage *m_pseudoSingerPage;
    AppearancePage *appearancePage;
    // G2pPage *g2pPage;
    InferencePage *inferencePage;
    DeveloperPage *developerPage;
    QList<IOptionPage *> pages;
};



#endif // APPOPTIONSDIALOG_H
