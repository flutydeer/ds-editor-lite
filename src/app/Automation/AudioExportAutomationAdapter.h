#ifndef AUDIOEXPORTAUTOMATIONADAPTER_H
#define AUDIOEXPORTAUTOMATIONADAPTER_H

#include "AudioExportAutomationFacade.h"

namespace Audio {
    class AudioExporterConfig;
}

namespace Automation {

    class AudioExportJobAdapter;

    AudioExportConfigDto toAutomationDto(const Audio::AudioExporterConfig &config);
    Audio::AudioExporterConfig fromAutomationDto(const AudioExportConfigDto &config);
    AudioExportRuntimeServices createAudioExportAutomationServices();

} // namespace Automation

#endif // AUDIOEXPORTAUTOMATIONADAPTER_H
