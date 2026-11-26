#ifndef MIDISYSTEM_H
#define MIDISYSTEM_H

#include <atomic>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>

#include <TalcsMidi/MidiMessageListener.h>

#include <Modules/Audio/utils/MidiPortList.h>

class QTimer;
class MidiInputNoteMonitor;

namespace talcs {

    class MixerAudioSource;

    class MidiInputDevice;
    class MidiMessageIntegrator;
    class MidiNoteSynthesizer;
}

class MidiSystem : public QObject {
    Q_OBJECT
public:
    explicit MidiSystem(QObject *parent = nullptr);
    ~MidiSystem() override;

    bool initialize();
    /// The port names of the current enumeration, or nothing when the
    /// enumeration failed, which is not the same as having no port.
    static std::optional<QStringList> availableDevices();

    talcs::MidiInputDevice *device() const;

    MidiInputNoteMonitor *hardwareNotes() const {
        return m_hardwareNotes;
    }
    bool isDeviceReady() const;
    /// The port this system keeps open. It is remembered while the port is
    /// unplugged, so the device is reopened once the port comes back.
    QString deviceId() const;
    bool setDeviceId(const QString &deviceId);
    QString deviceErrorString() const;
    /// The ports seen by the last poll.
    MidiPortList ports() const;

    /// Re-enumerates the ports now. The periodic poll runs this every
    /// MidiSystem::refreshIntervalMsec.
    void refreshDevices();

    talcs::MidiMessageIntegrator *integrator() const;
    talcs::MidiNoteSynthesizer *synthesizer() const;

    void setGenerator(int g) const;
    int generator() const;
    void setAmplitudeDecibel(double dB) const;
    double amplitudeDecibel() const;
    void setAttackMsec(int msec) const;
    int attackMsec() const;
    void setDecayMsec(int msec) const;
    int decayMsec() const;
    void setDecayRatio(double ratio) const;
    double decayRatio() const;
    void setReleaseMsec(int msec) const;
    int releaseMsec() const;
    void setFrequencyOfA(double frequency) const;
    double frequencyOfA() const;

    static void updateControl();

    /// How often the port list is polled. A shorter interval detects an
    /// unplugged device sooner, but every poll enumerates the MIDI ports.
    static constexpr int refreshIntervalMsec = 2000;

signals:
    /// The set of available ports changed.
    void devicesChanged();
    /// The open device changed: another device, a lost one, or an error.
    void deviceChanged();

private:
    /// Reports errors raised by the device on its own thread.
    class ErrorListener : public talcs::MidiMessageListener {
    public:
        explicit ErrorListener(MidiSystem *system);

        /// Marks the device this listener is attached to.
        void setGeneration(quint64 generation);

    protected:
        void processError(const QString &errorString) override;

    private:
        MidiSystem *m_system;
        /// Read on the device thread, written when a device is attached.
        std::atomic<quint64> m_generation{0};
    };

    std::unique_ptr<talcs::MidiInputDevice> m_device;
    std::unique_ptr<talcs::MidiMessageIntegrator> m_integrator;
    talcs::MidiNoteSynthesizer *m_synthesizer;
    std::unique_ptr<talcs::MixerAudioSource> m_synthesizerMixer;
    std::unique_ptr<ErrorListener> m_errorListener;
    MidiInputNoteMonitor *m_hardwareNotes;
    QTimer *m_refreshTimer = nullptr;
    MidiPortList m_ports;
    QString m_deviceId;
    QString m_error;
    /// Tells an error raised by a former device from one raised by the
    /// device that is open now.
    quint64 m_deviceGeneration = 0;

protected:
    /// Enumerates the ports. Exposed for tests that need a failing poll.
    virtual std::optional<QStringList> enumeratePorts();

private:
    void attachDevice();
    void handleDeviceError(const QString &errorString, quint64 generation);
    /// Enumerates the ports and publishes the list. Returns whether it changed.
    bool refreshPorts();
    /// Drops the open device, which stops the notes it keeps sounding.
    void releaseDevice();
    void dropDevice();

    void updateRateOnSampleRateChange(double sampleRate) const;
};


#endif // MIDISYSTEM_H
