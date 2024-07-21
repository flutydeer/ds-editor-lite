#ifndef DEVICETESTER_H
#define DEVICETESTER_H

#include <QObject>

#include <TalcsCore/AudioSource.h>
#include <TalcsCore/AudioBuffer.h>

class DeviceTester : public QObject, public talcs::AudioSource {
    Q_OBJECT
public:
    explicit DeviceTester(QObject *parent = nullptr);
    ~DeviceTester() override;

    bool open(qint64 bufferSize, double sampleRate) override;
    void close() override;

    void startTest();

    static void playTestSound();

protected:
    qint64 processReading(const talcs::AudioSourceReadData &readData) override;

private:
    talcs::AudioBuffer m_sound;
    QAtomicInteger<qint64> m_pos = -1;
};



#endif // DEVICETESTER_H
