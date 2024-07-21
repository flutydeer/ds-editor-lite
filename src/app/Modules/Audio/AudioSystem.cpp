#include "AudioSystem.h"

#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/subsystem/VSTConnectionSystem.h>
#include <Modules/Audio/subsystem/MidiSystem.h>

static AudioSystem *m_instance = nullptr;

AudioSystem::AudioSystem(bool isVST, QObject *parent) : QObject(parent), m_isVST(isVST) {
    m_instance = this;
    m_outputSystem = new OutputSystem(this);
    m_vstConnectionSystem = new VSTConnectionSystem(this);
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
VSTConnectionSystem *AudioSystem::vstConnectionSystem() {
    return m_instance->m_vstConnectionSystem;
}
AbstractOutputSystem *AudioSystem::sessionOutputSystem() {
    return m_instance->m_isVST ? static_cast<AbstractOutputSystem *>(m_instance->m_vstConnectionSystem) : static_cast<AbstractOutputSystem *>(m_instance->m_outputSystem);
}
bool AudioSystem::isVST() {
    return m_instance->m_isVST;
}
MidiSystem *AudioSystem::midiSystem() {
    return m_instance->m_midiSystem;
}