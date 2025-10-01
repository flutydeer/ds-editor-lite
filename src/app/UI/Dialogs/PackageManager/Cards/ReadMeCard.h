//
// Created by FlutyDeer on 2025/10/2.
//

#ifndef DS_EDITOR_LITE_READMECARD_H
#define DS_EDITOR_LITE_READMECARD_H

#include "UI/Controls/OptionsCard.h"

class ReadMeCard : public OptionsCard {
    Q_OBJECT

public:
    explicit ReadMeCard(QWidget *parent = nullptr);

public slots:
    void onDataContextChanged(const QString &dataContext);

private:
    QLabel *lbReadMe = nullptr;
};


#endif // DS_EDITOR_LITE_READMECARD_H
