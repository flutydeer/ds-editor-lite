//
// Created by fluty on 24-3-18.
//

#ifndef OPTIONSCARDITEM_H
#define OPTIONSCARDITEM_H

#include <QWidget>

class QHBoxLayout;
class QCheckBox;
class QLabel;
class OptionsCardItem : public QWidget {
    Q_OBJECT
public:
    explicit OptionsCardItem(QWidget *parent = nullptr);

    void setCheckable(bool checkable);
    void setChecked(bool checked);
    // void setEnabled(bool enabled);
    // void setCheckBoxEnabled(bool enabled);
    void setTitle(const QString &title);
    void setDescription(const QString &desc);
    void addWidget(QWidget *widget);
    void removeWidget(QWidget *widget);

private:
    QHBoxLayout *m_mainLayout;
    QCheckBox *m_checkBox;
    QLabel *m_lbTitle;
    QLabel *m_lbDesc;
};



#endif // OPTIONSCARDITEM_H
