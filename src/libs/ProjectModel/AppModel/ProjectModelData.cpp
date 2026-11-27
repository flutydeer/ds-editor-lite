#include <lite/ProjectModel/AppModel/ProjectModelData.h>

#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

ProjectModelData::ProjectModelData() = default;
ProjectModelData::~ProjectModelData() = default;
ProjectModelData::ProjectModelData(ProjectModelData &&) noexcept = default;
ProjectModelData &ProjectModelData::operator=(ProjectModelData &&) noexcept = default;

void ProjectModelData::setAudioReferenceDirectory(const QString &directory) {
    for (const auto &track : tracks) {
        for (auto *clip : track->clips()) {
            if (auto *audio = qobject_cast<AudioClip *>(clip))
                audio->setReferenceDirectory(directory);
        }
    }
}
