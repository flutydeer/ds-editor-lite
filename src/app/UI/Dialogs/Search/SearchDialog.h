#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidget>

#include "UI/Dialogs/Base/Dialog.h"
#include "Model/AppModel/SingingClip.h"

class SearchDialog final : public Dialog {
    Q_OBJECT
public:
    explicit SearchDialog(SingingClip *singingClip, QWidget *parent = nullptr);
    ~SearchDialog() override;

private Q_SLOTS:
    void onSearchTextChanged(const QString &searchTerm);
    void onItemSelectionChanged(int row) const;

private:
    SingingClip *m_clip;
    QList<Note *> m_notes;
    QLineEdit *lineEditSearch;
    QListWidget *resultListWidget;
    QLabel *labelInfo;
    QString searchText;
};

#endif // SEARCHDIALOG_H
