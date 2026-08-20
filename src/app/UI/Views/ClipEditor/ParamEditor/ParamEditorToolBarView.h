#ifndef PARAMEDITORTOOLBARVIEW_H
#define PARAMEDITORTOOLBARVIEW_H

#include <lite/ProjectModel/AppModel/Params.h>
#include "ParamEditorEditMode.h"
#include "SpeakerMixToolBarView.h"

#include <QWidget>

class ComboBox;
class ToolButton;
class IconLabel;
class QStackedWidget;
class SingingClip;
class ParamEditToolBarView;

class ParamEditorToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorToolBarView(QWidget *parent = nullptr);

    void setSpeakerMixMode(bool on);
    void setSpeakers(const QStringList &names, const QList<QColor> &colors);
    void setSpeakerMixDynamicState(SpeakerMixDynamicUiState state);
    void setBakeEnabled(bool enabled);

signals:
    void foregroundChanged(ParamInfo::Name name);
    void backgroundChanged(ParamInfo::Name name);
    void editModeChanged(ParamEditorEditMode mode);
    void previousKeyframe();
    void nextKeyframe();
    void bypassDynamicMix();
    void resumeDynamicMix();
    void stopDynamicMix();

private slots:
    void onForegroundSelectionChanged(int index);
    void onBackgroundSelectionChanged(int index);
    void onSwap() const;

private:
    void changeEvent(QEvent *event) override;
    void retranslateUi();

    SingingClip *m_clip = nullptr;
    IconLabel *lbForegroundParam;
    ComboBox *cbForegroundParam;
    IconLabel *lbBackgroundParam;
    ComboBox *cbBackgroundParam;
    ToolButton *m_btnSwap = nullptr;
    QStackedWidget *m_toolBarStack = nullptr;
    ParamEditToolBarView *m_paramEditToolBar = nullptr;
    SpeakerMixToolBarView *m_speakerMixToolBar = nullptr;
};



#endif // PARAMEDITORTOOLBARVIEW_H
