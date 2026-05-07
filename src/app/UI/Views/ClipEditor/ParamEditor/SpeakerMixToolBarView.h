//
// Created by fluty on 2026/5/4.
//

#ifndef SPEAKERMIXTOOLBARVIEW_H
#define SPEAKERMIXTOOLBARVIEW_H

#include "UI/Controls/Button.h"

#include <QWidget>

class QLabel;

class SpeakerMixToolBarView final : public QWidget {
    Q_OBJECT

public:
    explicit SpeakerMixToolBarView(QWidget *parent = nullptr);

    void setSpeakers(const QStringList &names, const QList<QColor> &colors);

signals:
    void previousKeyframe();
    void nextKeyframe();

private:
    Button *m_btnPrev;
    Button *m_btnNext;
    QWidget *m_speakerContainer;
};

#endif // SPEAKERMIXTOOLBARVIEW_H
