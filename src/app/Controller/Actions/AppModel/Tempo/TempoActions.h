//
// Created by fluty on 2024/2/7.
//

#ifndef TEMPOACTIONS_H
#define TEMPOACTIONS_H

#include "Modules/History/ActionSequence.h"

class AppModel;

class TempoActions : public ActionSequence {
public:
    void editTempo(double oldTempo, double newTempo, AppModel *model);

private:
    [[nodiscard]] static double tickToMs(int tick, double tempo);
    [[nodiscard]] static int msToTick(double ms, double tempo);
};



#endif // TEMPOACTIONS_H
