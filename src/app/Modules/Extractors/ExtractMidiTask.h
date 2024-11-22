//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include "ExtractTask.h"
#include <some-infer/Some.h>

class ExtractMidiTask final : public ExtractTask {
    Q_OBJECT

public:

    explicit ExtractMidiTask(Input input);

    void terminate() override;

    std::vector<Some::Midi> result;

private:
    void runTask() override;

    std::unique_ptr<Some::Some> m_some;
};
#endif // EXTRACTMIDITASK_H
