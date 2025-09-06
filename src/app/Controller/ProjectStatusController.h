//
// Created by fluty on 24-8-30.
//

#ifndef PROJECTSTATUSCONTROLLER_H
#define PROJECTSTATUSCONTROLLER_H

#include "ModelChangeHandler.h"
#include "Utils/Singleton.h"

class ProjectStatusController final : public ModelChangeHandler {
    Q_OBJECT

private:
    explicit ProjectStatusController() = default;
    ~ProjectStatusController() override = default;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ProjectStatusController)
    Q_DISABLE_COPY_MOVE(ProjectStatusController)

private:
    void handleTrackRemoved(Track *track) override;
    void handleTempoChanged(double tempo) override;
    void handleClipInserted(Clip *clip) override;
    void handleClipRemoved(Clip *clip) override;
    void handleClipPropertyChanged(Clip *clip) override;
    static void updateProjectEditableLength();
};



#endif // PROJECTSTATUSCONTROLLER_H
