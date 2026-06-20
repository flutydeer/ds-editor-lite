//
// Created by fluty on 2026/5/4.
//

#ifndef SPEAKERMIXTOOLBARVIEW_H
#define SPEAKERMIXTOOLBARVIEW_H

#include "UI/Controls/Button.h"

#include <QWidget>

class QLabel;
class SwitchButton;

class SpeakerMixToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit SpeakerMixToolBarView(QWidget *parent = nullptr);

    void setSpeakers(const QStringList &names, const QList<QColor> &colors);
    void setDynamicMixEnabled(bool enabled);
    void setDynamicMixChecked(bool checked);

signals:
    void previousKeyframe();
    void nextKeyframe();
    void dynamicMixToggled(bool checked);

private:
    QLabel *m_dynamicLabel = nullptr;
    SwitchButton *m_dynamicSwitch = nullptr;
    Button *m_btnPrev;
    Button *m_btnNext;
    QWidget *m_speakerContainer;
    QStringList m_speakerNames;
    QList<QColor> m_speakerColors;
    bool m_dynamicMixEnabled = false;
    bool m_dynamicMixChecked = false;
};

#endif // SPEAKERMIXTOOLBARVIEW_H
