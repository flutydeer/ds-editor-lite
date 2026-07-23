#ifndef IEDITORVIEW_H
#define IEDITORVIEW_H

#include "EditorViewState.h"
#include "Interface/HistoryFocus.h"
#include <lite/Support/Macros.h>

LITE_INTERFACE IEditorView {
    I_DECL(IEditorView)

    I_NODSCD(EditorViewState captureEditorViewState() const);
    I_METHOD(bool restoreEditorViewState(const EditorViewState &state));

    I_METHOD(bool centerTrackPanelAt(double tick, double trackIndex));
    I_METHOD(bool setTrackPanelScale(double horizontalScale, double verticalScale));
    I_METHOD(bool setEditorPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible));
    I_METHOD(bool showBottomPanelPage(const QString &pageId));

    I_METHOD(bool centerPianoRollAt(double tick, double keyIndex));
    I_METHOD(bool setPianoRollScale(double horizontalScale, double verticalScale));
    I_METHOD(bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode));

    I_METHOD(void refreshActiveClipTrackPresentation());
    I_METHOD(void previewActiveClipTrackColor(int colorIndex));
    I_METHOD(HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const);
    I_METHOD(bool revealFocus(const HistoryFocus &focus));
    I_METHOD(bool finalizeFocus(const HistoryFocus &focus));
    I_METHOD(void clearFocusPreview());
};

#endif // IEDITORVIEW_H
