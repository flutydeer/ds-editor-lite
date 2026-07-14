#ifndef PROJECTMODELDATA_H
#define PROJECTMODELDATA_H

#include "TimeSignature.h"
#include "TrackControl.h"

#include <memory>
#include <vector>

class Track;

struct ProjectModelData {
    ProjectModelData();
    ~ProjectModelData();
    ProjectModelData(ProjectModelData &&) noexcept;
    ProjectModelData &operator=(ProjectModelData &&) noexcept;

    ProjectModelData(const ProjectModelData &) = delete;
    ProjectModelData &operator=(const ProjectModelData &) = delete;

    double tempo = 120.0;
    TimeSignature timeSignature;
    TrackControl masterControl;
    std::vector<std::unique_ptr<Track>> tracks;
};

#endif // PROJECTMODELDATA_H
