//
// Created by fluty on 24-3-18.
//

#ifndef GENERALPAGE_H
#define GENERALPAGE_H

#include "IOptionPage.h"

class GeneralPage : public IOptionPage {
    Q_OBJECT

public:
    explicit GeneralPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
};



#endif // GENERALPAGE_H
