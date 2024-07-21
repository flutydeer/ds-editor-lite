#ifndef VSTCONNECTIONSYSTEM_H
#define VSTCONNECTIONSYSTEM_H

#include <QPointer>

#include <Modules/Audio/subsystem/AbstractOutputSystem.h>

namespace talcs {
    class MixerAudioSource;
    class RemoteOutputContext;
    class RemoteEditor;
    class RemoteMidiMessageIntegrator;
    class MidiNoteSynthesizer;
}

class VSTConnectionSystem : public AbstractOutputSystem {
    Q_OBJECT
public:
    explicit VSTConnectionSystem(QObject *parent = nullptr);
    ~VSTConnectionSystem() override;

    static bool createVSTConfig();

    bool initialize() override;

    inline talcs::RemoteOutputContext *remoteOutputContext() const {
        return m_remoteOutputContext.get();
    }

    void setApplicationInitializing(bool);
    bool isApplicationInitializing() const;

    talcs::RemoteEditor *remoteEditor() const;

    talcs::RemoteMidiMessageIntegrator *integrator() const;
    talcs::MidiNoteSynthesizer *synthesizer() const;

    void syncSynthesizerPreference();

    QPair<QString, QString> hostSpecs() const;

signals:
    void hostSpecsChanged(const QString &hostExecutable, const QString &pluginFormat);

private:
    std::unique_ptr<talcs::MidiNoteSynthesizer> m_synthesizer;
    std::unique_ptr<talcs::RemoteMidiMessageIntegrator> m_integrator;
    std::unique_ptr<talcs::MixerAudioSource> m_synthesizerMixer;

    std::unique_ptr<talcs::RemoteOutputContext> m_remoteOutputContext;

    std::unique_ptr<talcs::RemoteEditor> m_editor;
    bool m_isApplicationInitializing = false;

    QPair<QString, QString> m_hostSpecs;

    QByteArray getEditorData(bool *ok);
    bool setEditorData(const QByteArray &data);
    void handleSampleRateChange(double sampleRate);
    void setHostSpecs(const QString &hostExecutable, const QString &pluginFormat);
};

#endif // VSTCONNECTIONSYSTEM_H
