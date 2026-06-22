//
// Created by fluty on 2026/5/4.
//

#ifndef SPEAKERMIXTOOLBARVIEW_H
#define SPEAKERMIXTOOLBARVIEW_H

#include "UI/Controls/Button.h"

#include <QWidget>

enum class SpeakerMixDynamicUiState { Unavailable, NotEnabled, Active, Bypassed };

class SpeakerMixToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit SpeakerMixToolBarView(QWidget *parent = nullptr);

    void setSpeakers(const QStringList &names, const QList<QColor> &colors);
    void setDynamicState(SpeakerMixDynamicUiState state);

signals:
    void previousKeyframe();
    void nextKeyframe();
    void bypassDynamicMix();
    void resumeDynamicMix();
    void stopDynamicMix();

private:
    Button *m_btnBypass = nullptr;
    Button *m_btnResume = nullptr;
    Button *m_btnStop = nullptr;
    Button *m_btnPrev = nullptr;
    Button *m_btnNext = nullptr;
    QWidget *m_speakerContainer = nullptr;
    QStringList m_speakerNames;
    QList<QColor> m_speakerColors;
    SpeakerMixDynamicUiState m_dynamicState = SpeakerMixDynamicUiState::Unavailable;
};

#endif // SPEAKERMIXTOOLBARVIEW_H
