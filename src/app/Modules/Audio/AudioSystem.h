#ifndef DS_EDITOR_LITE_AUDIOSYSTEM_H
#define DS_EDITOR_LITE_AUDIOSYSTEM_H

#include <QObject>

#include <TalcsRemote/RemoteAudioDevice.h>

namespace talcs {
    class AudioDevice;
    class AudioDriver;
    class AudioDriverManager;
    class TransportAudioSource;
    class PositionableMixerAudioSource;
    class AudioSourcePlayback;
    class MixerAudioSource;
    class RemoteSocket;
    class TransportAudioSourceProcessInfoCallback;
}

class AudioSettingsDialog;
class AudioContext;

class AudioSystem : public QObject {
    Q_OBJECT
    class VstProcessInfoCallback : public talcs::RemoteAudioDevice::ProcessInfoCallback {
    public:
        void onThisBlockProcessInfo(const talcs::RemoteAudioDevice::ProcessInfo &processInfo) override;
        bool m_isPaused = true;
    };
public:
    explicit AudioSystem(QObject *parent = nullptr);
    ~AudioSystem() override;
    static AudioSystem *instance();

    bool initialize(bool isVstMode);
    bool findProperDriver();
    bool findProperDevice();

    [[nodiscard]] talcs::AudioDriverManager *driverManager() const;
    [[nodiscard]] talcs::AudioDevice *device() const;
    [[nodiscard]] talcs::AudioDriver *driver() const;
    [[nodiscard]] talcs::AudioSourcePlayback *playback() const;
    [[nodiscard]] talcs::MixerAudioSource *preMixer() const;
    [[nodiscard]] talcs::TransportAudioSource *transport() const;
    [[nodiscard]] talcs::PositionableMixerAudioSource *masterTrack() const;
    [[nodiscard]] talcs::RemoteSocket *socket() const;

    [[nodiscard]] bool isDeviceAutoClosed() const;

    [[nodiscard]] bool checkStatus() const;

    bool setDriver(const QString &driverName);
    bool setDevice(const QString &deviceName);

    [[nodiscard]] qint64 adoptedBufferSize() const;
    void setAdoptedBufferSize(qint64 bufferSize);
    [[nodiscard]] double adoptedSampleRate() const;
    void setAdoptedSampleRate(double sampleRate);

    void testDevice();

    enum HotPlugMode {
        NotifyOnAnyChange,
        NotifyOnCurrentRemoval,
        None,
    };

    [[nodiscard]] AudioContext *audioContext() const;

    static QString driverDisplayName(const QString &driverName);

private:

    talcs::AudioDriverManager *m_drvMgr;
    talcs::AudioDriver *m_drv = nullptr;
    std::unique_ptr<talcs::AudioDevice> m_dev;
    QScopedPointer<talcs::AudioSourcePlayback> m_playback;
    talcs::TransportAudioSource *m_tpSrc;
    talcs::PositionableMixerAudioSource *m_masterTrack;

    talcs::MixerAudioSource *m_preMixer;

    bool m_isDeviceAutoClosed = false;

    qint64 m_adoptedBufferSize = 0;
    double m_adoptedSampleRate = 0.0;

    void postSetDevice();

    void handleDeviceHotPlug();

    talcs::RemoteSocket *m_socket = nullptr;
    talcs::RemoteAudioDevice *m_remoteDev = nullptr;
    QScopedPointer<VstProcessInfoCallback> m_remoteTpCb;
    static QPair<quint16, quint16> checkVstConfig() ;

    AudioContext *m_audioContext;

};



#endif // DS_EDITOR_LITE_AUDIOSYSTEM_H
