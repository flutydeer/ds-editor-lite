#ifndef OUTPUTSYSTEM_H
#define OUTPUTSYSTEM_H

#include <memory>

#include <TalcsDevice/OutputContext.h>

#include <Modules/Audio/subsystem/AbstractOutputSystem.h>

class OutputSystem : public AbstractOutputSystem {
    Q_OBJECT
public:
    explicit OutputSystem(QObject *parent = nullptr);
    ~OutputSystem() override;

    bool initialize() override;

    inline talcs::OutputContext *outputContext() const {
        return m_outputContext.get();
    }

    bool setDriver(const QString &driverName) const;
    bool setDevice(const QString &deviceName) const;
    bool setAdoptedBufferSize(qint64 bufferSize) const;
    bool setAdoptedSampleRate(double sampleRate) const;
    void setHotPlugNotificationMode(talcs::OutputContext::HotPlugNotificationMode mode) const;

private:
    void postSetDevice() const;

    std::unique_ptr<talcs::OutputContext> m_outputContext;
};



#endif // OUTPUTSYSTEM_H
