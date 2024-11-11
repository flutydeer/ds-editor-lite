#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QRadioButton> // 引入 QRadioButton

#include "UI/Dialogs/Base/Dialog.h"
#include "Model/AppModel/SingingClip.h"

#include <QCheckBox>

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
