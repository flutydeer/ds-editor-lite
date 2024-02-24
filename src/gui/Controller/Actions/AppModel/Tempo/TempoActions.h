//
// Created by fluty on 2024/2/7.
//

#ifndef TEMPOACTIONS_H
#define TEMPOACTIONS_H

#include "Controller/History/ActionSequence.h"

class AppModel;

class TempoActions : public ActionSequence{
public:
    void editTempo(double oldTempo, double newTempo, AppModel *model);
};



#endif //TEMPOACTIONS_H
