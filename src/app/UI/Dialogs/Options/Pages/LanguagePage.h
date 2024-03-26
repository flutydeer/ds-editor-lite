#ifndef LANGUAGEPAGE_H
#define LANGUAGEPAGE_H

#include "IOptionPage.h"

#include <Modules/Language/LangSetting/Controls/LangListWidget.h>
#include <Modules/Language/LangSetting/Controls/LangInfoWidget.h>
#include <Modules/Language/LangSetting/Controls/G2pInfoWidget.h>

class OptionsCardItem;

class LanguagePage final : public IOptionPage {
    Q_OBJECT
public:
    explicit LanguagePage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;

private:
    LangSetting::LangListWidget *m_langListWidget;
    LangSetting::LangInfoWidget *m_langInfoWidget;
    LangSetting::G2pInfoWidget *m_g2pInfoWidget;
};

#endif // LANGUAGEPAGE_H
