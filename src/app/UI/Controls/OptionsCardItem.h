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

    void setTitle(const QString &title) const;
    void setDescription(const QString &desc) const;
    void addWidget(QWidget *widget) const;
    void removeWidget(QWidget *widget) const;

private:
    QHBoxLayout *m_mainLayout;
    QLabel *m_lbTitle;
    QLabel *m_lbDesc;
};

#endif // OPTIONSCARDITEM_H
