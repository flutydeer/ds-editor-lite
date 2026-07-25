//
// Created by fluty on 24-3-18.
//

#ifndef OPTIONSCARD_H
#define OPTIONSCARD_H

#include <QWidget>


class CardView;
class QLabel;

class OptionsCard : public QWidget {
    Q_OBJECT

public:
    explicit OptionsCard(QWidget *parent = nullptr);

    void setTitle(const QString &title) const;
    CardView *card() const;

private:
    using QWidget::setLayout;

    QLabel *m_lbTitle;
    CardView *m_card;
};



#endif // OPTIONSCARD_H
