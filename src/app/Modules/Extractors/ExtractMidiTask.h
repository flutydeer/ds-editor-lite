//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include "ExtractTask.h"
#include <game-infer/Game.h>

class ExtractMidiTask final : public ExtractTask {
    Q_OBJECT

public:
    explicit ExtractMidiTask(Input input);

    void terminate() override;

    std::vector<Game::GameMidi> result;

private:
    void runTask() override;

    std::unique_ptr<Game::Game> m_game;
};
#endif // EXTRACTMIDITASK_H
