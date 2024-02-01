#ifndef DS_EDITOR_LITE_AUDIOSYSTEM_H
#define DS_EDITOR_LITE_AUDIOSYSTEM_H

#include <QObject>
#include <QSettings>

namespace talcs {
    class AudioDevice;
    class AudioDriver;
    class AudioDriverManager;
    class TransportAudioSource;
    class PositionableMixerAudioSource;
    class AudioSourcePlayback;
    class MixerAudioSource;
}

class AudioSettingsDialog;

class AudioSystem : public QObject {
    Q_OBJECT
public:
    explicit AudioSystem(QObject *parent = nullptr);
    ~AudioSystem() override;
    static AudioSystem *instance();

    bool initialize(bool isVstMode);

    talcs::AudioDriverManager *driverManager() const;
    talcs::AudioDevice *device() const;
    talcs::AudioDriver *driver() const;
    talcs::TransportAudioSource *transport() const;

    bool isDeviceAutoClosed() const;

    bool checkStatus() const;

    bool setDriver(const QString &driverName);
    bool setDevice(const QString &deviceName);

    qint64 adoptedBufferSize() const;
    void setAdoptedBufferSize(qint64 bufferSize);
    double adoptedSampleRate() const;
    void setAdoptedSampleRate(double sampleRate);

    void openAudioSettings();

    void testDevice();

    enum HotPlugMode {
        NotifyOnAnyChange,
        NotifyOnCurrentRemoval,
        None,
    };

    static QString driverDisplayName(const QString &driverName);

private:
    friend class AudioSettingsDialog;

    QSettings m_settings;

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

    bool findProperDriver();
    bool findProperDevice();
    void postSetDevice();

    void handleDeviceHotPlug();
};



#endif // DS_EDITOR_LITE_AUDIOSYSTEM_H
