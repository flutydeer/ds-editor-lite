#ifndef APPEARANCEPAGE_H
#define APPEARANCEPAGE_H

#include "IOptionPage.h"


class SwitchButton;
class ComboBox;
class LineEdit;

class AppearancePage : public IOptionPage {
    Q_OBJECT

public:
    explicit AppearancePage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    void changeTheme(int index);
    void changeInterfaceFont(int index);

    SwitchButton *m_swUseNativeFrame;
    ComboBox *m_cbxTheme;
    ComboBox *m_cbxInterfaceFont;
    SwitchButton *m_swAnimationEnabled;
    LineEdit *m_leAnimationTimeScale;
#if defined(WITH_DIRECT_MANIPULATION)
    SwitchButton *m_swEnableDirectManipulation;
#endif
    QStringList m_fontFamilies;
};


#endif // APPEARANCEPAGE_H
