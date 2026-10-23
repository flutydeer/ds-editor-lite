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
    /// The text the page itself put into the lyric field for the current language. A field that
    /// still holds it was not edited, and must not be stored: writing back what the page merely
    /// displays would turn an effective value into an explicit override.
    QString m_displayedLyric;
    /// The table entry that the displayed text came from. Editing the field back to the displayed
    /// text restores this entry, so an intermediate edit cannot leave an override behind that would
    /// shadow a value a singer states later.
    QString m_storedLyric;
    /// The default lyrics this page changed, keyed by language. A language mapped to an empty string
    /// asks for the default again. The page applies these entries to the table as it stands instead
    /// of writing its whole table back, so an entry another surface changes while the dialog is open
    /// is neither resurrected nor overwritten.
    QMap<QString, QString> m_pendingLyrics;

    PathEditor *m_packageSearchPaths;

    ComboBox *m_cbNoteAnalyzer;
    ComboBox *m_cbPitchAnalyzer;
    FileSelector *m_fsLibreSVIPPath;

    SwitchButton *m_swFingerDrawParam;
};

#endif // GENERALPAGE_H
