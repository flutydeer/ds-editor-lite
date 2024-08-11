//
// Created by fluty on 24-3-18.
//

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "IOptionPage.h"

class Button;
class LineEdit;
class LanguageComboBox;
class GeneralPage : public IOptionPage {
    Q_OBJECT

public:
    explicit GeneralPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;

private:
    Button *m_btnOpenConfigFolder;
    LanguageComboBox *m_cbDefaultSingingLanguage;
    LineEdit *m_leDefaultLyric;
};

#endif // GENERALPAGE_H
