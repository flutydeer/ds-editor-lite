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
    /// Baseline displayed text used to distinguish an actual edit from an unchanged effective
    /// value.
    QString m_displayedLyric;
    /// Sparse language edits; empty values remove overrides.
    QMap<QString, QString> m_pendingLyrics;

    PathEditor *m_packageSearchPaths;

    ComboBox *m_cbNoteAnalyzer;
    ComboBox *m_cbPitchAnalyzer;
    FileSelector *m_fsLibreSVIPPath;

    SwitchButton *m_swFingerDrawParam;
};

#endif // GENERALPAGE_H
