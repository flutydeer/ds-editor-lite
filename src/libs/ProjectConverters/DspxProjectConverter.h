#ifndef DSPXPROJECTCONVERTER_H
#define DSPXPROJECTCONVERTER_H

#include <lite/ProjectConverters/IProjectConverter.h>

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
    // Publishes the loaded project's loop region to the host after a complete load.
    virtual void applyLoadedLoopSettings(const LoopSettings &loopSettings) {
        Q_UNUSED(loopSettings);
    }
    // Supplies the host-owned loop snapshot to persist on save.
    virtual LoopSettings loopSettingsToSave() const {
        return {};
    }
};

#endif // DSPXPROJECTCONVERTER_H
