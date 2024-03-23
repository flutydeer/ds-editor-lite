//
// Created by fluty on 24-3-16.
//

#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

class AudioPage;
class AppearancePage;
class LanguagePage;
class GeneralPage;
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
        Appearance,
        Language,
        Save,
        Inference,
        PreviewFunctions,
        DeveloperOptions
    };

    explicit AppOptionsDialog(Page page, QWidget *parent = nullptr);

private slots:
    void onSelectionChanged(int index);

private:
    QStringList m_pageNames = {tr("General"),          tr("Audio"),
                               tr("Appearance"),       tr("Language"),
                            /* tr("Save"),               tr("Inference"),
                             * tr("Preview Functions"),  tr("Developer Options")*/};

    QListWidget *m_tabList;
    QStackedWidget *m_PageContent;

    GeneralPage *m_generalPage;
    AudioPage *m_audioPage;
    AppearancePage *m_appearancePage;
    LanguagePage *m_languagePage;
    QList<IOptionPage *> m_pages;
};



#endif // APPOPTIONSDIALOG_H
