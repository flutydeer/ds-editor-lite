//
// Created by fluty on 24-3-18.
//

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "IOptionPage.h"

class Button;
class ComboBox;
class LineEdit;
class LanguageComboBox;
class DirSelector;
class FileSelector;
class PathEditor;

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
#if false
    DirSelector *m_fsDefaultPackage;
    LineEdit *m_leDefaultSingerId;
    LineEdit *m_leDefaultSpeakerId;
#endif

    PathEditor *m_packageSearchPaths;

    FileSelector *m_fsSomePath;
    FileSelector *m_fsRmvpePath;
};

#endif // GENERALPAGE_H
