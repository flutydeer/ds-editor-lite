//
// Created by hrukalive on 2/7/24.
//

#ifndef DSPXPROJECTCONVERTER_H
#define DSPXPROJECTCONVERTER_H

#include "IProjectConverter.h"

#include <lite/ProjectModel/AppModel/LoopSettings.h>

namespace opendspx {
    struct Model;
}

using ImportMode = IProjectConverter::ImportMode;

class DspxProjectConverter : public IProjectConverter {
public:
    bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) override;
    bool loadParsedProject(const opendspx::Model &dspxModel, AppModel *model,
                           LoopSettings &loopSettings, QString &errMsg, ImportMode mode);
    bool save(const QString &path, AppModel *model, QString &errMsg) override;

protected:
    // Publish the loaded project's loop region. The base ignores it; the app
    // overrides this to push it into AppStatus, keeping the converter free of
    // app-runtime state.
    virtual void applyLoadedLoopSettings(const LoopSettings &loopSettings) {
        Q_UNUSED(loopSettings);
    }
    // The loop region to persist on save. The base has none; the app overrides
    // this to read the active loop region from AppStatus.
    virtual LoopSettings loopSettingsToSave() const {
        return {};
    }
};

#endif // DSPXPROJECTCONVERTER_H
