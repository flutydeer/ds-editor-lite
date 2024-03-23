#ifndef LANGUAGEPAGE_H
#define LANGUAGEPAGE_H

#include "IOptionPage.h"

#include "Modules/Language/Controls/LangListWidget.h"
#include "Modules/Language/Controls/LangInfoWidget.h"
#include "Modules/Language/Controls/G2pInfoWidget.h"

class OptionsCardItem;

class LanguagePage final: public IOptionPage {
    Q_OBJECT
public:
    explicit LanguagePage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;

private:
    LangMgr::LangListWidget *m_langListWidget;
    LangMgr::LangInfoWidget *m_langInfoWidget;
    LangMgr::G2pInfoWidget *m_g2pInfoWidget;
};

#endif // LANGUAGEPAGE_H
