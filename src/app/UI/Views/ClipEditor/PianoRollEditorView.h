#ifndef PIANOROLLEDITORVIEW_H
#define PIANOROLLEDITORVIEW_H

#include "Interface/EditorViewState.h"

#include <lite/GUI/Controls/OverlaySplitter.h>


class SingingClip;
class ParamEditorView;
class PianoRollView;

class PianoRollEditorView : public OverlaySplitter {
    Q_OBJECT
public:
    explicit PianoRollEditorView(QWidget *parent = nullptr);
    [[nodiscard]] PianoRollView *pianoRollView() const;
    [[nodiscard]] ParamEditorView *paramEditorView() const;
    [[nodiscard]] bool regionVisible(EditorViewGlobal::Region region) const;
    void setDataContext(SingingClip *clip) const;
    bool setRegionVisibility(bool pianoRollVisible, bool parametersVisible);
    bool showRegion(EditorViewGlobal::Region region);
    bool focusRegion(EditorViewGlobal::Region region);

private:
    PianoRollView *m_pianoRollView;
    ParamEditorView *m_paramEditorView;
};



#endif // PIANOROLLEDITORVIEW_H
