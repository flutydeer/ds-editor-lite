//
// Created by fluty on 24-3-13.
//

#ifndef APPEARANCEOPTIONCONTROLLER_H
#define APPEARANCEOPTIONCONTROLLER_H

#include "IOptionsController.h"
#include "Model/AppOptions/Options/AppearanceOption.h"

class AppearanceOptionController : public IOptionsController {
    Q_OBJECT

public:
    explicit AppearanceOptionController();
    void setOption(const AppearanceOption &option);

    void accept() override;
    void cancel() override;
    void update() override;

private:
    AppearanceOption m_temp;

    void updateTemp();
};

#endif // APPEARANCEOPTIONCONTROLLER_H
