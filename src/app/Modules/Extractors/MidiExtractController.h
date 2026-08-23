#ifndef MIDIEXTRACTOR_H
#define MIDIEXTRACTOR_H

#define midiExtractController MidiExtractController::instance()

#include "Controller/ModelChangeHandler.h"
#include <lite/Core/Singleton.h>


class SingingClip;
class AudioClip;

class MidiExtractController final : public ModelChangeHandler {
    Q_OBJECT

private:
    explicit MidiExtractController(QObject *parent = nullptr);
    ~MidiExtractController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(MidiExtractController)
    Q_DISABLE_COPY_MOVE(MidiExtractController)

public:
    void runExtractMidi(const AudioClip *audioClip);
};

#endif // MIDIEXTRACTOR_H
