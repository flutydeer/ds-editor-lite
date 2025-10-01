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

private:
    PseudoSingerPageWidget *m_widget;
};



#endif // PSEUDOSINGERPAGE_H
