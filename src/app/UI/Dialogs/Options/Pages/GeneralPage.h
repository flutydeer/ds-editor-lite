//
// Created by fluty on 24-3-18.
//

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "IOptionPage.h"

class Button;
class LineEdit;
class LanguageComboBox;
class FileSelector;

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
    LineEdit *m_leDefaultSinger;

    FileSelector *m_somePath;
    FileSelector *m_rmvpePath;
};

#endif // GENERALPAGE_H
