//
// Created by fluty on 24-3-16.
//

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

private:
    SwitchButton *m_swUseNativeFrame;
    ComboBox *m_cbxAnimationLevel;
    LineEdit *m_leAnimationTimeScale;
#if defined(WITH_DIRECT_MANIPULATION)
    SwitchButton *m_swEnableDirectManipulation;
#endif
    const QStringList animationLevelsName = {tr("Full"), tr("Decreased"), tr("None")};

    void modifyOption() override;
};



#endif // APPEARANCEPAGE_H
