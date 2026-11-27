#pragma once

#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioDriver.h>

namespace TestSupport {
    class FixtureAudioDevice final : public talcs::AudioDevice {
    public:
        FixtureAudioDevice(talcs::AudioDriver *driver, const QString &name) : AudioDevice(driver) {
            setName(name);
            setDriver(driver);
            setChannelCount(2);
            setActiveChannelCount(2);
            setAvailableBufferSizes({256, 512, 1024});
            setPreferredBufferSize(512);
            setAvailableSampleRates({44100, 48000});
            setPreferredSampleRate(48000);
            setIsInitialized(true);
        }

        bool open(qint64 bufferSize, double sampleRate) override {
            if (!availableBufferSizes().contains(bufferSize) ||
                !availableSampleRates().contains(sampleRate))
                return false;
            return talcs::AudioDevice::open(bufferSize, sampleRate);
        }

        bool openControlPanel() override {
            ++controlPanelRequests;
            return true;
        }

        int controlPanelRequests = 0;
    };

    class FixtureAudioDriver final : public talcs::AudioDriver {
    public:
        explicit FixtureAudioDriver(bool defaultDeviceAvailable)
            : defaultDeviceAvailable(defaultDeviceAvailable) {
            setName(QStringLiteral("fixture-output"));
        }

        bool initialize() override {
            return available && AudioDriver::initialize();
        }

        QStringList devices() const override {
            return availableDevices;
        }

        QString defaultDevice() const override {
            return defaultDeviceAvailable ? devices().first() : QString();
        }

        talcs::AudioDevice *createDevice(const QString &name) override {
            return devices().contains(name) && name != unavailableDevice
                       ? new FixtureAudioDevice(this, name)
                       : nullptr;
        }

        QStringList availableDevices{QStringLiteral("Output A"), QStringLiteral("Output B")};
        bool available = true;
        QString unavailableDevice;

    private:
        const bool defaultDeviceAvailable;
    };
}
