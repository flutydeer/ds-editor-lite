//
// Created by fluty on 24-9-16.
//

#ifndef PIANOROLLEDITORVIEW_H
#define PIANOROLLEDITORVIEW_H

#include <QSplitter>


class SingingClip;
class ParamEditorView;
class PianoRollView;

class PianoRollEditorView : public QSplitter {
    Q_OBJECT
public:
    explicit PianoRollEditorView(QWidget *parent = nullptr);
    [[nodiscard]] PianoRollView *pianoRollView() const;
    [[nodiscard]] ParamEditorView *paramEditorView() const;
    void setDataContext(SingingClip *clip) const;

private:
    PianoRollView *m_pianoRollView;
    ParamEditorView *m_paramEditorView;
};



#endif // PIANOROLLEDITORVIEW_H
