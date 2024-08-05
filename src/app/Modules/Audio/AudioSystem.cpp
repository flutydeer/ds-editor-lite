#include "AudioSystem.h"

#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/subsystem/MidiSystem.h>

static AudioSystem *m_instance = nullptr;

AudioSystem::AudioSystem(QObject *parent) : QObject(parent) {
    m_instance = this;
    m_outputSystem = new OutputSystem(this);
    m_midiSystem = new MidiSystem(this);
}
AudioSystem::~AudioSystem() {
    m_instance = nullptr;
}
AudioSystem *AudioSystem::instance() {
    return m_instance;
}
OutputSystem *AudioSystem::outputSystem() {
    return m_instance->m_outputSystem;
}
MidiSystem *AudioSystem::midiSystem() {
    return m_instance->m_midiSystem;
}