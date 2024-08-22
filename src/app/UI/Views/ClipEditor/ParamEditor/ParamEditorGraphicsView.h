//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORGRAPHICSVIEW_H
#define PARAMEDITORGRAPHICSVIEW_H

#include "UI/Views/Common/TimeGraphicsView.h"


class ParamEditorGraphicsScene;

class ParamEditorGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit ParamEditorGraphicsView(ParamEditorGraphicsScene *scene, QWidget *parent = nullptr);
};



#endif // PARAMEDITORGRAPHICSVIEW_H
