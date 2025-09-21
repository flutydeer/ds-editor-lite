#ifndef G2PPAGE_H
#define G2PPAGE_H

#include "IOptionPage.h"

#include <Modules/Language/LangSetting/Controls/G2pListWidget.h>
#include <Modules/Language/LangSetting/Controls/G2pInfoWidget.h>

class OptionsCardItem;

class G2pPage final : public IOptionPage {
    Q_OBJECT
public:
    explicit G2pPage(QWidget *parent = nullptr);

    void update() const;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    LangSetting::G2pListWidget *m_g2pListWidget;
    LangSetting::G2pInfoWidget *m_g2pInfoWidget;
};



#endif // G2PPAGE_H
