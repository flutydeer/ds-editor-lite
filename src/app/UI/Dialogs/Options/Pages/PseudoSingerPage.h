#ifndef PSEUDOSINGERPAGE_H
#define PSEUDOSINGERPAGE_H

#include "IOptionPage.h"

class PseudoSingerPageWidget;

class PseudoSingerPage : public IOptionPage {
    Q_OBJECT
public:
    explicit PseudoSingerPage(QWidget *parent = nullptr);
    ~PseudoSingerPage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    PseudoSingerPageWidget *m_widget;
};



#endif // PSEUDOSINGERPAGE_H
