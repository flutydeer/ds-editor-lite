//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORVIEW_H
#define PARAMEDITORVIEW_H

#include "Model/AppModel/Params.h"

#include <QWidget>

class SingingClip;
class ParamEditorGraphicsView;

class ParamEditorView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip) const;
    [[nodiscard]] ParamEditorGraphicsView *graphicsView() const;

public slots:
    void onParamChanged(ParamInfo::Name name, Param::Type type) const;

private:
    SingingClip *m_clip = nullptr;
    ParamEditorGraphicsView *m_graphicsView;
    ParamInfo::Name m_foregroundParam = ParamInfo::Breathiness;
    ParamInfo::Name m_backgroundParam = ParamInfo::Tension;
};



#endif // PARAMEDITORVIEW_H
