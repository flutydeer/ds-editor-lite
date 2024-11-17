//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include "Modules/Task/Task.h"
#include <some-infer/Some.h>

class ExtractMidiTask final : public Task {
    Q_OBJECT

public:
    struct Input {
        int audioClipId = -1;
        QString audioPath;
        double tempo = 0;
    };

    explicit ExtractMidiTask(Input input);

    void terminate() override;

    int audioClipId = -1;
    bool success = false;
    const Input &input() const;
    std::vector<Some::Midi> result;

private:
    void runTask() override;

    Input m_input;
    std::unique_ptr<Some::Some> m_some;
};
#endif // EXTRACTMIDITASK_H
