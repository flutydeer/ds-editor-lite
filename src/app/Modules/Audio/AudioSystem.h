#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <QObject>

class AbstractOutputSystem;
class OutputSystem;
class MidiSystem;

class AudioSystem : public QObject {
    Q_OBJECT
public:
    explicit AudioSystem(QObject *parent = nullptr);
    ~AudioSystem() override;

    Q_DISABLE_COPY_MOVE(AudioSystem)

    static AudioSystem *instance();

    static OutputSystem *outputSystem();
    static MidiSystem *midiSystem();

private:
    OutputSystem *m_outputSystem = nullptr;
    MidiSystem *m_midiSystem;
};



#endif // AUDIOSYSTEM_H
