#include "AudioSystem.h"

#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/subsystem/MidiSystem.h>

static AudioSystem *s_instance = nullptr;

AudioSystem::AudioSystem(QObject *parent) : QObject(parent) {
    assert(s_instance == nullptr && "AudioSystem instance already exists");
    s_instance = this;
    m_outputSystem = new OutputSystem(this);
    m_midiSystem = new MidiSystem(this);
}

AudioSystem::~AudioSystem() {
    s_instance = nullptr;
}

AudioSystem *AudioSystem::instance() {
    return s_instance;
}

OutputSystem *AudioSystem::outputSystem() {
    return s_instance->m_outputSystem;
}

MidiSystem *AudioSystem::midiSystem() {
    return s_instance->m_midiSystem;
}