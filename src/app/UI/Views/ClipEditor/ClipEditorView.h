//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "PianoRollEditorView.h"
#include "Interface/EditorViewState.h"
#include "Modules/History/HistoryFocus.h"
#include "UI/Views/Common/TabPanelPage.h"
#include "UI/Views/Common/PanelView.h"

class QLabel;
class QSplitter;
class ParamEditorView;
class PianoRollView;
class ClipEditorToolBarView;
class Track;
class Curve;
class Clip;
class SingingClip;
class AudioClip;

class ClipEditorView final : public TabPanelPage {
    Q_OBJECT

public:
    [[nodiscard]] QString tabId() const override;
    [[nodiscard]] QString tabName() const override;
    [[nodiscard]] AppGlobal::PanelType panelType() const override;
    [[nodiscard]] QWidget *toolBar() override;
    [[nodiscard]] QWidget *content() override;
    [[nodiscard]] bool isToolBarVisible() const override;

    explicit ClipEditorView(QWidget *parent = nullptr);

    [[nodiscard]] PianoRollViewState viewState() const;
    [[nodiscard]] bool supportsEditMode(EditorViewGlobal::PianoRollEditMode mode) const;
    bool centerAt(double tick, double keyIndex) const;
    bool setViewScale(double horizontalScale, double verticalScale) const;
    bool setEditMode(EditorViewGlobal::PianoRollEditMode mode);
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus, bool animated) const;
    void refreshActiveClipTrackPresentation();
    void previewActiveClipTrackColor(int colorIndex) const;

public slots:
    void onModelChanged();
    void onActiveClipChanged(int clipId);

private:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveToSingingClipState(SingingClip *clip) const;
    void moveToAudioClipState(const AudioClip *clip) const;
    void moveToNullClipState() const;

    ClipEditorToolBarView *m_toolbarView;
    PianoRollEditorView *m_pianoRollEditorView;
    QLabel *m_placeholderLabel;
    mutable bool m_hasActiveClip = false;
    QMetaObject::Connection m_trackColorConnection;

    void reset();
    void applyTrackColor(int colorIndex) const;
};



#endif // CLIPEDITVIEW_H
