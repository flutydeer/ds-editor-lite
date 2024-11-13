//
// Created by fluty on 24-11-13.
//

#ifndef PITCHEXTRACTOR_H
#define PITCHEXTRACTOR_H

#define pitchExtractController PitchExtractController::instance()

#include "Controller/ModelChangeHandler.h"
#include "Utils/Singleton.h"

#include <QObject>


class ExtractPitchTask;
class SingingClip;
class AudioClip;

class PitchExtractController :public ModelChangeHandler, public Singleton<PitchExtractController> {
    Q_OBJECT

public:
    void runExtractPitch(AudioClip *audioClip, SingingClip *singingClip);

private slots:
    static void onExtractPitchTaskFinished(ExtractPitchTask *task);
};

#endif // PITCHEXTRACTOR_H
