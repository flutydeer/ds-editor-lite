#ifndef DS_EDITOR_LITE_AUDIOSYSTEM_H
#define DS_EDITOR_LITE_AUDIOSYSTEM_H

#include <QObject>

namespace talcs {
    class AudioDevice;
    class AudioDriver;
    class AudioDriverManager;
    class TransportAudioSource;
    class PositionableMixerAudioSource;
    class AudioSourcePlayback;
}

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

    bool checkStatus() const;

    bool setDriver(const QString &driverName);
    bool setDevice(const QString &deviceName);

    void openAudioSettings();

private:
    talcs::AudioDriverManager *m_drvMgr;
    talcs::AudioDriver *m_drv = nullptr;
    std::unique_ptr<talcs::AudioDevice> m_dev;
    QScopedPointer<talcs::AudioSourcePlayback> m_playback;
    talcs::TransportAudioSource *m_tpSrc;
    talcs::PositionableMixerAudioSource *m_masterTrack;

    bool findProperDevice();
};



#endif // DS_EDITOR_LITE_AUDIOSYSTEM_H
