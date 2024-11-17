//
// Created by fluty on 24-11-13.
//

#ifndef MIDIEXTRACTOR_H
#define MIDIEXTRACTOR_H

#define midiExtractController MidiExtractController::instance()

#include "Controller/ModelChangeHandler.h"
#include "Utils/Singleton.h"

#include <QObject>


class ExtractMidiTask;
class SingingClip;
class AudioClip;

class MidiExtractController final : public ModelChangeHandler, public Singleton<MidiExtractController> {
    Q_OBJECT

public:
    void runExtractMidi(const AudioClip *audioClip);

private slots:
    static void onExtractMidiTaskFinished(ExtractMidiTask *task);
};

#endif // MIDIEXTRACTOR_H
