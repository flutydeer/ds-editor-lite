//
// Created by fluty on 2024/2/7.
//

#ifndef TEMPOACTIONS_H
#define TEMPOACTIONS_H

#include <lite/History/ActionSequence.h>

#include <lite/MusicBase/Tempo.h>

class AppModel;

class TempoActions : public ActionSequence {
public:
    void setTempoAt(const Tempo &tempo, AppModel *model);
    void removeTempoAt(int tick, AppModel *model);
};



#endif // TEMPOACTIONS_H
