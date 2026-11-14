#include "MidiSynthesizerIntegrator.h"

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

MidiSynthesizerIntegrator::MidiSynthesizerIntegrator(talcs::MidiNoteSynthesizer *synthesizer,
                                                   bool takeOwnership)
    : m_synthesizer(synthesizer) {
    setStream(synthesizer, takeOwnership);
}

bool MidiSynthesizerIntegrator::open(qint64 bufferSize, double sampleRate) {
    QMutexLocker locker(&m_mutex);
    return talcs::MidiMessageIntegrator::open(bufferSize, sampleRate);
}

void MidiSynthesizerIntegrator::close() {
    QMutexLocker locker(&m_mutex);
    talcs::MidiMessageIntegrator::close();
}

void MidiSynthesizerIntegrator::interruptAllNotes(bool force) {
    QMutexLocker locker(&m_mutex);
    talcs::MidiMessageIntegrator::flush();
    m_synthesizer->noteSynthesizer()->flush(force);
}

qint64 MidiSynthesizerIntegrator::processReading(const talcs::AudioSourceReadData &readData) {
    QMutexLocker locker(&m_mutex);
    return talcs::MidiMessageIntegrator::processReading(readData);
}

bool MidiSynthesizerIntegrator::processMessage(const talcs::MidiMessage &message) {
    QMutexLocker locker(&m_mutex);
    return talcs::MidiMessageIntegrator::processMessage(message);
}

bool MidiSynthesizerIntegrator::processDeviceWillStart(talcs::MidiInputDevice *device) {
    QMutexLocker locker(&m_mutex);
    return talcs::MidiMessageIntegrator::processDeviceWillStart(device);
}

void MidiSynthesizerIntegrator::processDeviceStopped() {
    QMutexLocker locker(&m_mutex);
    talcs::MidiMessageIntegrator::processDeviceStopped();
}

void MidiSynthesizerIntegrator::processError(const QString &errorString) {
    QMutexLocker locker(&m_mutex);
    talcs::MidiMessageIntegrator::processError(errorString);
}
