//
// Created by fluty on 24-8-30.
//

#ifndef PROJECTSTATUSCONTROLLER_H
#define PROJECTSTATUSCONTROLLER_H

#include "ModelChangeHandler.h"
#include "Utils/Singleton.h"

class ProjectStatusController final : public ModelChangeHandler,
                                      public Singleton<ProjectStatusController> {
    Q_OBJECT

public:
    explicit ProjectStatusController() = default;

private:
    void handleTempoChanged(double tempo) override;
    void handleClipInserted(Clip *clip) override;
    void handleClipRemoved(Clip *clip) override;
    void handleClipPropertyChanged(Clip *clip) override;
    static void updateProjectEditableLength();
};



#endif // PROJECTSTATUSCONTROLLER_H
