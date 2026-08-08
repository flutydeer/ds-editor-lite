#ifndef DS_EDITOR_LITE_PROJECTIMPORTCONFIGDIALOG_H
#define DS_EDITOR_LITE_PROJECTIMPORTCONFIGDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

#include <QScopedPointer>

class ProjectImportConfigDialogPrivate;

// Generic single-page configuration container for project imports. The
// format-specific page widget is injected via setPage(); OK / Cancel are
// provided by the container itself.
class ProjectImportConfigDialog : public Dialog {
    Q_OBJECT
    Q_DECLARE_PRIVATE(ProjectImportConfigDialog)
public:
    explicit ProjectImportConfigDialog(QWidget *parent = nullptr);
    ~ProjectImportConfigDialog() override;

    void setPage(QWidget *page);
    [[nodiscard]] QWidget *page() const;

private:
    QScopedPointer<ProjectImportConfigDialogPrivate> d_ptr;
};

#endif // DS_EDITOR_LITE_PROJECTIMPORTCONFIGDIALOG_H
