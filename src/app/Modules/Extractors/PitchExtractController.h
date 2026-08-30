#ifndef PITCHEXTRACTOR_H
#define PITCHEXTRACTOR_H

#define pitchExtractController PitchExtractController::instance()

#include "Controller/ModelChangeHandler.h"
#include <lite/Core/Singleton.h>


class SingingClip;
class AudioClip;

class PitchExtractController final : public ModelChangeHandler {
    Q_OBJECT

private:
    explicit PitchExtractController(QObject *parent = nullptr);
    ~PitchExtractController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(PitchExtractController)
    Q_DISABLE_COPY_MOVE(PitchExtractController)

public:
    void runExtractPitch(const AudioClip *audioClip, const SingingClip *singingClip);
};

#endif // PITCHEXTRACTOR_H
