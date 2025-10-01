//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "PianoRollEditorView.h"
#include "Interface/IClipEditorView.h"
#include "UI/Views/Common/ITabPanelPage.h"
#include "UI/Views/Common/PanelView.h"

class QSplitter;
class ParamEditorView;
class PianoRollView;
class ClipEditorToolBarView;
class Track;
class Curve;
class Clip;
class SingingClip;
class AudioClip;

class ClipEditorView final : public QWidget, public ITabPanelPage, public IClipEditorView {
    Q_OBJECT

public:
    QString tabId() const override;
    QString tabName() const override;
    AppGlobal::PanelType panelType() const override;
    QWidget *toolBar() override;
    QWidget *content() override;

    explicit ClipEditorView(QWidget *parent = nullptr);

    void centerAt(double tick, double keyIndex) override;
    void centerAt(double startTick, double length, double keyIndex) override;

public slots:
    void onModelChanged() const;
    void onActiveClipChanged(int clipId) const;

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveToSingingClipState(SingingClip *clip) const;
    void moveToAudioClipState(const AudioClip *clip) const;
    void moveToNullClipState() const;

    ClipEditorToolBarView *m_toolbarView;
    PianoRollEditorView *m_pianoRollEditorView;

    void reset() const;
    // void printParts();
};



#endif // CLIPEDITVIEW_H
