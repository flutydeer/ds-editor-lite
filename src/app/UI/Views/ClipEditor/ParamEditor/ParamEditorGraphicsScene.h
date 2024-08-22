//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORGRAPHICSSCENE_H
#define PARAMEDITORGRAPHICSSCENE_H

#include "UI/Views/Common/TimeGraphicsScene.h"

class ParamEditorGraphicsScene : public TimeGraphicsScene {
    Q_OBJECT

public:
    explicit ParamEditorGraphicsScene(QObject *parent = nullptr);

public slots:
    void onViewResized(QSize size);

private:
    void updateSceneRect() override;
    QSize m_viewSize = QSize(0, 0);
};



#endif // PARAMEDITORGRAPHICSSCENE_H
