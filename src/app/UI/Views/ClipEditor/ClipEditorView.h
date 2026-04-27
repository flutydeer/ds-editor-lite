//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "PianoRollEditorView.h"
#include "Interface/IClipEditorView.h"
#include "UI/Views/Common/TabPanelPage.h"
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

class ClipEditorView final : public TabPanelPage, public IClipEditorView {
    Q_OBJECT

public:
    [[nodiscard]] QString tabId() const override;
    [[nodiscard]] QString tabName() const override;
    [[nodiscard]] AppGlobal::PanelType panelType() const override;
    [[nodiscard]] QWidget *toolBar() override;
    [[nodiscard]] QWidget *content() override;
    [[nodiscard]] bool isToolBarVisible() const override;

    explicit ClipEditorView(QWidget *parent = nullptr);

    void centerAt(double tick, double keyIndex) override;
    void centerAt(double startTick, double length, double keyIndex) override;

public slots:
    void onModelChanged();
    void onActiveClipChanged(int clipId);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveToSingingClipState(SingingClip *clip) const;
    void moveToAudioClipState(const AudioClip *clip) const;
    void moveToNullClipState() const;

    ClipEditorToolBarView *m_toolbarView;
    PianoRollEditorView *m_pianoRollEditorView;
    mutable bool m_hasActiveClip = false;
    QMetaObject::Connection m_trackColorConnection;

    void reset();
    // void printParts();
};



#endif // CLIPEDITVIEW_H
