//
// Created by fluty on 24-3-16.
//

#ifndef APPEARANCEPAGE_H
#define APPEARANCEPAGE_H

#include "IOptionPage.h"

class ComboBox;
class LineEdit;

class AppearancePage : public IOptionPage {
    Q_OBJECT

public:
    explicit AppearancePage(QWidget *parent = nullptr);

private:
    ComboBox *m_cbxAnimationLevel;
    LineEdit *m_leAnimationTimeScale;

    const QStringList animationLevelsName = {tr("Full"), tr("Decreased"), tr("None")};

    void modifyOption() override;
};



#endif // APPEARANCEPAGE_H
