//
// Created by fluty on 24-9-16.
//

#ifndef PARAMEDITORTOOLBARVIEW_H
#define PARAMEDITORTOOLBARVIEW_H

#include "Model/AppModel/Params.h"

#include <QWidget>

class ComboBox;
class Button;
class QLabel;
class SingingClip;
class SpeakerMixToolBarView;

class ParamEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorToolBarView(QWidget *parent = nullptr);

    void setSpeakerMixMode(bool on);
    void setSpeakers(const QStringList &names, const QList<QColor> &colors);
    void setDynamicMixEnabled(bool enabled);
    void setDynamicMixChecked(bool checked);

signals:
    void foregroundChanged(ParamInfo::Name name);
    void backgroundChanged(ParamInfo::Name name);
    void previousKeyframe();
    void nextKeyframe();
    void dynamicMixToggled(bool checked);

private slots:
    void onForegroundSelectionChanged(int index);
    void onBackgroundSelectionChanged(int index);
    void onSwap() const;

private:
    SingingClip *m_clip = nullptr;
    QLabel *lbForegroundParam;
    ComboBox *cbForegroundParam;
    QLabel *lbBackgroundParam;
    ComboBox *cbBackgroundParam;
    SpeakerMixToolBarView *m_speakerMixToolBar = nullptr;
};



#endif // PARAMEDITORTOOLBARVIEW_H
