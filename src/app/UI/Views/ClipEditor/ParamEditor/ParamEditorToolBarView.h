//
// Created by fluty on 24-9-16.
//

#ifndef PARAMEDITORTOOLBARVIEW_H
#define PARAMEDITORTOOLBARVIEW_H

#include "Model/AppModel/Params.h"

#include <QWidget>

class SingingClip;

class ParamEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorToolBarView(QWidget *parent = nullptr);

signals:
    void foregroundChanged(ParamInfo::Name name);
    void backgroundChanged(ParamInfo::Name name);

private slots:
    void onForegroundSelectionChanged(int index);
    void onBackgroundSelectionChanged(int index);

private:
    SingingClip *m_clip;
};



#endif // PARAMEDITORTOOLBARVIEW_H
