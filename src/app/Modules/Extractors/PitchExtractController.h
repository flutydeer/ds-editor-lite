//
// Created by fluty on 24-11-13.
//

#ifndef PITCHEXTRACTOR_H
#define PITCHEXTRACTOR_H

#define pitchExtractController PitchExtractController::instance()

#include "Controller/ModelChangeHandler.h"
#include "Utils/Singleton.h"


class ExtractPitchTask;
class SingingClip;
class AudioClip;

class PitchExtractController final : public ModelChangeHandler,
                                     public Singleton<PitchExtractController> {
    Q_OBJECT
public:
    void runExtractPitch(const AudioClip *audioClip, const SingingClip *singingClip);

private slots:
    static void onExtractPitchTaskFinished(ExtractPitchTask *task);
};

#endif // PITCHEXTRACTOR_H
