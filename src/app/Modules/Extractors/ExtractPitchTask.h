//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include "ExtractTask.h"
#include <rmvpe-infer/Rmvpe.h>

class ExtractPitchTask final : public ExtractTask {
    Q_OBJECT

public:
    explicit ExtractPitchTask(Input input);

    void terminate() override;

    QList<QPair<double, QList<double>>> result;

private:
    void runTask() override;
    static std::vector<float> freqToMidi(const std::vector<float> &frequencies);
    QList<double> processOutput(const QList<double> &values) const;

    std::unique_ptr<Rmvpe::Rmvpe> m_rmvpe;
};
#endif // EXTRACTPITCHTASK_H
