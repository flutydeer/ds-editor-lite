//
// Created by fluty on 24-10-31.
//

#ifndef OPTIONLISTCARD_H
#define OPTIONLISTCARD_H

#include "OptionsCard.h"

class OptionsCardItem;
class QVBoxLayout;

class OptionListCard : public OptionsCard {
    Q_OBJECT

public:
    explicit OptionListCard(QWidget *parent = nullptr);
    explicit OptionListCard(QString title, QWidget *parent = nullptr);

    void addItem(OptionsCardItem *item);
    void addItem(const QString &title, QWidget *control);
    void addItem(const QString &title, const QList<QWidget *> &controls);
    void addItem(const QString &title, const QString &description, QWidget *control);
    void addItem(const QString &title, const QString &description,
                 const QList<QWidget *> &controls);

private:
    using OptionsCard::card;
    void initUi();
    QString m_title = "Card Title";
    QVBoxLayout *m_cardLayout = nullptr;
    int m_itemCount = 0;
};



#endif // OPTIONLISTCARD_H
