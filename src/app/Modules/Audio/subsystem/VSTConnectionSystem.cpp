#include "VSTConnectionSystem.h"

#include <QApplication>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QFile>

#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/Decibels.h>
#include <TalcsDevice/AudioSourcePlayback.h>
#include <TalcsRemote/RemoteSocket.h>
#include <TalcsRemote/RemoteAudioDevice.h>
#include <TalcsRemote/RemoteOutputContext.h>
#include <TalcsRemote/RemoteEditor.h>
#include <TalcsRemote/RemoteMidiMessageIntegrator.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include <Model/AppOptions/AppOptions.h>

static double msecToRate(int msec, double sampleRate) {
    if (msec == 0)
        return 0.005;
    return std::pow(0.005, 1000.0 / (msec * sampleRate));
}

VSTConnectionSystem::VSTConnectionSystem(QObject *parent) : AbstractOutputSystem(parent) {
    m_remoteOutputContext = std::make_unique<talcs::RemoteOutputContext>();
    setContext(m_remoteOutputContext.get());

    m_integrator = std::make_unique<talcs::RemoteMidiMessageIntegrator>();
    m_synthesizer = std::make_unique<talcs::MidiNoteSynthesizer>();
    m_integrator->setStream(m_synthesizer.get());
    m_synthesizerMixer = std::make_unique<talcs::MixerAudioSource>();
    m_synthesizerMixer->addSource(m_integrator.get());

    context()->preMixer()->addSource(m_integrator.get());
}
VSTConnectionSystem::~VSTConnectionSystem() = default;

static QString appDataDir() {
    static const auto path = []() {
        QString path;
        QString slashName;
#ifdef Q_OS_WINDOWS
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#elif defined(Q_OS_MAC)
        path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config";
#else
        path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
        slashName = "/" + QApplication::applicationName();
        if (path.endsWith(slashName)) {
            path.chop(slashName.size());
        }
        slashName = "/" + QApplication::organizationName();
        if (path.endsWith(slashName)) {
            path.chop(slashName.size());
        }
        return path;
    }();
    return path+ "/" + QApplication::organizationName() + "/" + QApplication::applicationName();
}

bool VSTConnectionSystem::createVSTConfig() {
    auto vstConfigPath = appDataDir() + "/vstconfig.json";
    qDebug() << "Audio::VSTConnectionSystem: VST config path" << vstConfigPath;
    QFile vstConfigFile(vstConfigPath);
    if (!vstConfigFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Audio::VSTConnectionSystem: fatal: cannot open VST config file";
        return false;
    }
    QJsonDocument doc({
        {"editor",      QApplication::applicationFilePath()},
        {"editorPort",  appOptions->audio()->vstEditorPort},
        {"pluginPort",  appOptions->audio()->vstPluginPort},
        {"threadCount", QThread::idealThreadCount()},
        {"theme",       appOptions->audio()->vstPluginEditorUsesCustomTheme ? QJsonValue(appOptions->audio()->vstTheme) : QJsonValue::Null},
    });
    vstConfigFile.write(doc.toJson());
    return true;
}
bool VSTConnectionSystem::initialize() {
    if (!createVSTConfig())
        return false;
    auto editorPort = appOptions->audio()->vstEditorPort;
    auto pluginPort = appOptions->audio()->vstPluginPort;

    setFileBufferingReadAheadSize(appOptions->audio()->fileBufferingReadAheadSize);
    if (!m_remoteOutputContext->initialize(editorPort, pluginPort)) {
        qWarning() << "Audio::VSTConnectionSystem: fatal: socket fails to start server";
        return false;
    }
    syncSynthesizerPreference();
    m_editor = std::make_unique<talcs::RemoteEditor>(
        m_remoteOutputContext->socket(),
        [this](bool *ok) { return getEditorData(ok); },
        [this](const QByteArray &data) { return setEditorData(data); });
    m_remoteOutputContext->socket()->bind("vstConnectionSystem", "setHostSpecs", [=](const std::string &hostExecutable, const std::string &pluginFormat) {
        setHostSpecs(QString::fromStdString(hostExecutable), QString::fromStdString(pluginFormat));
    });
    if (!m_remoteOutputContext->establishConnection()) {
        qWarning() << "Audio::VSTConnectionSystem: fatal: socket fails to start client";
        return false;
    }
    qDebug() << "Audio::VSTConnectionSystem: waiting for connection" << "editorPort =" << editorPort << "pluginPort =" << pluginPort;
    return true;
}
void VSTConnectionSystem::setApplicationInitializing(bool a) {
    m_isApplicationInitializing = a;
}
bool VSTConnectionSystem::isApplicationInitializing() const {
    return m_isApplicationInitializing;
}
talcs::RemoteEditor *VSTConnectionSystem::remoteEditor() const {
    return m_editor.get();
}
talcs::RemoteMidiMessageIntegrator *VSTConnectionSystem::integrator() const {
    return m_integrator.get();
}
talcs::MidiNoteSynthesizer *VSTConnectionSystem::synthesizer() const {
    return m_synthesizer.get();
}
void VSTConnectionSystem::syncSynthesizerPreference() {
    auto dev = m_remoteOutputContext->device();
    m_synthesizer->noteSynthesizer()->setGenerator(static_cast<talcs::NoteSynthesizer::Generator>(appOptions->audio()->midiSynthesizerGenerator));
    m_synthesizer->noteSynthesizer()->setAttackRate(msecToRate(appOptions->audio()->midiSynthesizerAttackMsec, dev && dev->isOpen() ? dev->sampleRate() : 48000));
    m_synthesizer->noteSynthesizer()->setReleaseRate(msecToRate(appOptions->audio()->midiSynthesizerReleaseMsec, dev && dev->isOpen() ? dev->sampleRate() : 48000));
    m_synthesizerMixer->setGain(talcs::Decibels::decibelsToGain(appOptions->audio()->midiSynthesizerAmplitude));
    if (qFuzzyIsNull(appOptions->audio()->midiSynthesizerFrequencyOfA)) {
        // TODO
    } else {
        m_synthesizer->setFrequencyOfA(appOptions->audio()->midiSynthesizerFrequencyOfA);
    }
}
QPair<QString, QString> VSTConnectionSystem::hostSpecs() const {
    return m_hostSpecs;
}
QByteArray VSTConnectionSystem::getEditorData(bool *ok) {
    return QByteArray(); // TODO
}
bool VSTConnectionSystem::setEditorData(const QByteArray &data) {
    return false; // TODO
}
void VSTConnectionSystem::handleSampleRateChange(double sampleRate) {
    m_synthesizer->noteSynthesizer()->setAttackRate(msecToRate(appOptions->audio()->midiSynthesizerAttackMsec, sampleRate));
    m_synthesizer->noteSynthesizer()->setReleaseRate(msecToRate(appOptions->audio()->midiSynthesizerReleaseMsec, sampleRate));
}
void VSTConnectionSystem::setHostSpecs(const QString &hostExecutable, const QString &pluginFormat) {
    m_hostSpecs = {hostExecutable, pluginFormat};
    emit hostSpecsChanged(hostExecutable, pluginFormat);
}