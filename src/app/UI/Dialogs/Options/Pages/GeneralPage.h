#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "IOptionPage.h"

#include <QMap>

class ComboBox;
class LineEdit;
class LanguageComboBox;
class DirSelector;
class FileSelector;
class PathEditor;
class SwitchButton;

class GeneralPage : public IOptionPage {
    Q_OBJECT

public:
    explicit GeneralPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    ComboBox *m_cbUiLanguage;
    LanguageComboBox *m_cbDefaultSingingLanguage;
    LineEdit *m_leDefaultLyric;
    QString m_previousLanguage;
    QMap<QString, QString> m_defaultLyrics;

    PathEditor *m_packageSearchPaths;

    FileSelector *m_fsGameDir;
    FileSelector *m_fsRmvpePath;
    FileSelector *m_fsLibreSVIPPath;

    SwitchButton *m_swFingerDrawParam;
};

#endif // GENERALPAGE_H
