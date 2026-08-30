#ifndef PRESETAUTOMATIONFACADE_H
#define PRESETAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <QDateTime>
#include <QVersionNumber>
#include <QVector>

#include <functional>

namespace Automation {

    struct SpeakerMixPresetSourceDto {
        QString speakerId;
        QString speakerName;

        friend bool operator==(const SpeakerMixPresetSourceDto &,
                               const SpeakerMixPresetSourceDto &) = default;
    };

    struct SpeakerMixPresetDto {
        QString id;
        QString name;
        QString packageId;
        QString singerId;
        QVersionNumber packageVersion;
        QList<SpeakerMixPresetSourceDto> sources;
        QVector<double> fixedWeights;
        QDateTime createdAt;
        QDateTime updatedAt;

        friend bool operator==(const SpeakerMixPresetDto &, const SpeakerMixPresetDto &) = default;
    };

    struct PresetRuntimeServices {
        std::function<QList<SpeakerMixPresetDto>()> speakerMixPresets;
        std::function<bool(const QList<SpeakerMixPresetDto> &)> applySpeakerMixPresets;
    };

    class PresetAutomationFacade final {
    public:
        PresetAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                               PresetRuntimeServices services = {});

        AutomationResult<QList<SpeakerMixPresetDto>> getSpeakerMixPresets();
        AutomationResult<SpeakerMixPresetDto>
            saveSpeakerMixPreset(const ApplicationCommandContext &context,
                                 SpeakerMixPresetDto preset);
        AutomationResult<ApplicationMutationResult>
            deleteSpeakerMixPreset(const ApplicationCommandContext &context,
                                   const QString &presetId);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        PresetRuntimeServices m_services;
    };

} // namespace Automation

#endif // PRESETAUTOMATIONFACADE_H
