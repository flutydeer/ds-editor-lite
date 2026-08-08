#ifndef DS_EDITOR_LITE_IPROJECTCONFIGPAGE_H
#define DS_EDITOR_LITE_IPROJECTCONFIGPAGE_H

class QWidget;

// Format-specific configuration page hosted by ProjectImportConfigDialog.
// The page renders its own controls and reports the collected configuration
// as a UserInput DTO; it never touches the session or the document.
class IProjectConfigPage {
public:
    virtual ~IProjectConfigPage() = default;

    virtual QWidget *widget() = 0;
};

#endif // DS_EDITOR_LITE_IPROJECTCONFIGPAGE_H
