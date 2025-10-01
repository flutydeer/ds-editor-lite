//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORVIEW_H
#define PARAMEDITORVIEW_H

#include "Model/AppModel/ParamProperties.h"
#include "Model/AppModel/Params.h"

#include <QWidget>

class ParamEditorInfoArea;
class SingingClip;
class ParamEditorGraphicsView;

class ParamEditorView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip) const;
    ParamEditorGraphicsView *graphicsView() const;

public slots:
    void onForegroundChanged(ParamInfo::Name name) const;
    void onBackgroundChanged(ParamInfo::Name name) const;

private:
    SingingClip *m_clip = nullptr;
    ParamEditorGraphicsView *m_graphicsView;
    ParamEditorInfoArea *m_infoArea;
};

#endif // PARAMEDITORVIEW_H
