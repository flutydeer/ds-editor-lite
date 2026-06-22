//
// Created by fluty on 24-8-21.
//

#ifndef PARAMEDITORVIEW_H
#define PARAMEDITORVIEW_H

#include "Model/AppModel/ParamProperties.h"
#include "Model/AppModel/Params.h"
#include "Model/AppModel/SpeakerMixData.h"

#include <QWidget>

class ParamEditorInfoArea;
class SingingClip;
class ParamEditorGraphicsView;
class ParamEditorToolBarView;
class Button;
class QLabel;
class QVBoxLayout;

class ParamEditorView final : public QWidget {
    Q_OBJECT

public:
    explicit ParamEditorView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);
    [[nodiscard]] ParamEditorGraphicsView *graphicsView() const;

public slots:
    void onForegroundChanged(ParamInfo::Name name);
    void onBackgroundChanged(ParamInfo::Name name) const;

private slots:
    void onPreviousKeyframe() const;
    void onNextKeyframe() const;
    void onSpeakerMixEdited(const SpeakerMixModel::SpeakerMixData &data) const;
    void onEnableDynamicMix();
    void onBypassDynamicMix() const;
    void onResumeDynamicMix() const;
    void onStopDynamicMix();
    void refreshSpeakerMixToolBar();

private:
    void refreshSpeakerMixEmptyState(const SpeakerMixModel::SpeakerMixData &data);
    void setSpeakerMixEmptyState(const QString &title, const QString &message,
                                 const QString &buttonText, bool buttonEnabled);
    void hideSpeakerMixEmptyState();
    void updateSpeakerMixEmptyStateGeometry();
    static bool hasFixedMixBase(const SpeakerMixModel::SpeakerMixData &data);
    static SpeakerMixModel::SpeakerMixData dataWithDynamicEnabled(
        const SpeakerMixModel::SpeakerMixData &data);
    static SpeakerMixModel::SpeakerMixData dataWithDynamicStopped(
        const SpeakerMixModel::SpeakerMixData &data);

    SingingClip *m_clip = nullptr;
    ParamEditorGraphicsView *m_graphicsView;
    ParamEditorInfoArea *m_infoArea;
    ParamEditorToolBarView *m_toolBar;
    QWidget *m_speakerMixEmptyState = nullptr;
    QVBoxLayout *m_speakerMixEmptyLayout = nullptr;
    QLabel *m_speakerMixEmptyTitle = nullptr;
    QLabel *m_speakerMixEmptyMessage = nullptr;
    Button *m_enableDynamicMixButton = nullptr;
    QString m_speakerMixEmptyMessageText;
};

#endif // PARAMEDITORVIEW_H
