//
// Created by fluty on 24-3-18.
//

#ifndef OPTIONPAGE_H
#define OPTIONPAGE_H

#include <QScrollArea>

class IOptionPage : public QScrollArea {
    Q_OBJECT

public:
    explicit IOptionPage(QWidget *parent = nullptr);

protected:
    virtual void modifyOption() = 0;
    virtual QWidget *createContentWidget() = 0;
    void changeEvent(QEvent *event) override;
    void initializePage();

private:
    bool m_retranslatePending = false;
};

#endif // OPTIONPAGE_H
