//
// Created by fluty on 24-10-31.
//

#ifndef OPTIONLISTCARD_H
#define OPTIONLISTCARD_H

#include "OptionsCard.h"

#include <QHash>

class DividerLine;
class OptionsCardItem;
class QVBoxLayout;

class OptionListCard : public OptionsCard {
    Q_OBJECT

public:
    explicit OptionListCard(QWidget *parent = nullptr);
    explicit OptionListCard(QString title, QWidget *parent = nullptr);

    OptionsCardItem *addItem(OptionsCardItem *item);
    OptionsCardItem *addItem(const QString &title, QWidget *control);
    OptionsCardItem *addItem(const QString &title, const QString &description);
    OptionsCardItem *addItem(const QString &title, const QList<QWidget *> &controls);
    OptionsCardItem *addItem(const QString &title, const QString &description, QWidget *control);
    OptionsCardItem *addItem(const QString &title, const QString &description,
                             const QList<QWidget *> &controls);
    void setItemVisible(OptionsCardItem *item, bool visible);

private:
    using OptionsCard::card;
    void initUi();
    QString m_title = "Card Title";
    QVBoxLayout *m_cardLayout = nullptr;
    QHash<OptionsCardItem *, DividerLine *> m_itemDividers;
    int m_itemCount = 0;
};



#endif // OPTIONLISTCARD_H
