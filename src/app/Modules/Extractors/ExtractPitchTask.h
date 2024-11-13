//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include "Modules/Task/Task.h"

class ExtractPitchTask : public Task {
    Q_OBJECT

public:
    struct Input {
        int singingClipId = -1;
        int audioClipId = -1;
        QString audioPath;
        double tempo = 0;
    };

    explicit ExtractPitchTask(const Input &input);

    int singingClipId = -1;
    int audioClipId = -1;
    bool success = false;
    const Input &input() const;
    QList<double> result;

private:
    void runTask() override;
    static std::vector<float> freqToMidi(const std::vector<float> &frequencies);
    void processOutput(const QList<double> &values);

    Input m_input;
};
#endif // EXTRACTPITCHTASK_H
