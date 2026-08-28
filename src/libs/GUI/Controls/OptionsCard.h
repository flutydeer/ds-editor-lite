#ifndef OPTIONSCARD_H
#define OPTIONSCARD_H

#include <QWidget>


class CardView;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

class OptionsCard : public QWidget {
    Q_OBJECT

public:
    explicit OptionsCard(QWidget *parent = nullptr);

    void setTitle(const QString &title) const;
    void setTitleVisible(bool visible) const;
    void addTitleWidget(QWidget *widget) const;
    CardView *card() const;

private:
    using QWidget::setLayout;

    QLabel *m_lbTitle;
    QHBoxLayout *m_titleLayout;
    QVBoxLayout *m_mainLayout;
    CardView *m_card;
};



#endif // OPTIONSCARD_H
