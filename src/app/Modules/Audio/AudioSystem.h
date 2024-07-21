#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <QObject>

class AbstractOutputSystem;
class OutputSystem;
class VSTConnectionSystem;
class MidiSystem;

class AudioSystem : public QObject {
    Q_OBJECT
public:
    explicit AudioSystem(bool isVST, QObject *parent = nullptr);
    ~AudioSystem() override;
    static AudioSystem *instance();

    static OutputSystem *outputSystem();
    static VSTConnectionSystem *vstConnectionSystem();
    static bool isVST();
    static AbstractOutputSystem *sessionOutputSystem();
    static MidiSystem *midiSystem();

private:
    OutputSystem *m_outputSystem;
    VSTConnectionSystem *m_vstConnectionSystem;
    MidiSystem *m_midiSystem;
    bool m_isVST;
};



#endif // AUDIOSYSTEM_H
