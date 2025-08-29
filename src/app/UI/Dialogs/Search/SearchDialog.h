#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>

#include "UI/Dialogs/Base/Dialog.h"
#include "Model/AppModel/SingingClip.h"

class SearchDialog final : public Dialog {
    Q_OBJECT
public:
    explicit SearchDialog(SingingClip *singingClip, QWidget *parent = nullptr);
    ~SearchDialog() override;

private Q_SLOTS:
    void onSearchTextChanged();
    void onItemSelectionChanged(int row) const;
    void onPrevClicked() const;
    void onNextClicked() const;

private:
    void updateButtonState() const;

    SingingClip *m_clip;
    QList<Note *> m_notes;
    QLineEdit *lineEditSearch;
    QListWidget *resultListWidget;

    QLabel *labelInfo;
    QPushButton *btnPrev;
    QPushButton *btnNext;

    QRadioButton *startWithRadioButton;
    QRadioButton *fullSearchRadioButton;
    QRadioButton *fuzzySearchRadioButton;
    QCheckBox *caseSensitiveCheckBox;
    QCheckBox *regexCheckBox;
    QString searchText;
};

#endif // SEARCHDIALOG_H