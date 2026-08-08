#include "ProjectImportConfigDialog.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>

#include <QAbstractButton>
#include <QVBoxLayout>

class ProjectImportConfigDialogPrivate {
    Q_DECLARE_PUBLIC(ProjectImportConfigDialog)
public:
    explicit ProjectImportConfigDialogPrivate(ProjectImportConfigDialog *q) : q_ptr(q) {
    }

    void init() {
        Q_Q(ProjectImportConfigDialog);
        contentLayout = new QVBoxLayout(q->body());

        auto *okButton = new AccentButton(ProjectImportConfigDialog::tr("OK"));
        q->setPositiveButton(okButton);
        auto *cancelButton = new Button(ProjectImportConfigDialog::tr("Cancel"));
        q->setNegativeButton(cancelButton);
        QObject::connect(okButton, &QAbstractButton::clicked, q, &Dialog::accept);
        QObject::connect(cancelButton, &QAbstractButton::clicked, q, &Dialog::reject);
    }

    ProjectImportConfigDialog *q_ptr{};
    QVBoxLayout *contentLayout = nullptr;
};

ProjectImportConfigDialog::ProjectImportConfigDialog(QWidget *parent)
    : Dialog(parent), d_ptr(new ProjectImportConfigDialogPrivate(this)) {
    Q_D(ProjectImportConfigDialog);
    d->init();
}

ProjectImportConfigDialog::~ProjectImportConfigDialog() = default;

void ProjectImportConfigDialog::setPage(QWidget *page) {
    Q_D(ProjectImportConfigDialog);
    if (d->contentLayout->indexOf(page) >= 0)
        return;
    d->contentLayout->addWidget(page, 1);
}

QWidget *ProjectImportConfigDialog::page() const {
    Q_D(const ProjectImportConfigDialog);
    if (d->contentLayout->count() == 0)
        return nullptr;
    return d->contentLayout->itemAt(0)->widget();
}
