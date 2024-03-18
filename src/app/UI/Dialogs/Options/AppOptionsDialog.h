//
// Created by fluty on 24-3-16.
//

#ifndef APPOPTIONSDIALOG_H
#define APPOPTIONSDIALOG_H

#include "UI/Dialogs/Base/OKCancelApplyDialog.h"


class AudioPage;
class AppearancePage;
class GeneralPage;
class IOptionPage;
class QListWidget;
class Button;
class QStackedWidget;

class AppOptionsDialog : public OKCancelApplyDialog {
    Q_OBJECT

public:
    enum Page { General, Audio, Appearance, Save, Inference, PreviewFunctions, DeveloperOptions };

    explicit AppOptionsDialog(Page page, QWidget *parent = nullptr);

private slots:
    void apply();
    void cancel();
    void onSelectionChanged(int index);

private:
    QStringList m_pageNames = {tr("General"),          tr("Audio"),
                               tr("Appearance")/*,       tr("Save"),
                               tr("Inference"),        tr("Preview Functions"),
                               tr("Developer Options")*/};

    QListWidget *m_tabList;
    QStackedWidget *m_PageContent;

    GeneralPage *m_generalPage;
    AudioPage *m_audioPage;
    AppearancePage *m_appearancePage;
    QList<IOptionPage *> m_pages;
};



#endif // APPOPTIONSDIALOG_H
