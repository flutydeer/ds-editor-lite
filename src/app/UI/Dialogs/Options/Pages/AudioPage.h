//
// Created by fluty on 24-3-18.
//

#ifndef AUDIOPAGE_H
#define AUDIOPAGE_H

#include "IOptionPage.h"

class AudioPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AudioPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
};



#endif // AUDIOPAGE_H
