//
// Created by fluty on 24-2-17.
//

#ifndef CONTROLLERGLOBAL_H
#  define CONTROLLERGLOBAL_H

#endif // CONTROLLERGLOBAL_H

namespace ControllerGlobal {
    enum ElemType { LoopStart, LoopEnd, Tempo, TimeSignature, Track, Clip, NoteWithParams, None };
    inline QStringList ElemMimeType{
        "application/vnd.ds-editor-lite.loopstart", "application/vnd.ds-editor-lite.loopend",
        "application/vnd.ds-editor-lite.tempo",     "application/vnd.ds-editor-lite.timesignature",
        "application/vnd.ds-editor-lite.track",     "application/vnd.ds-editor-lite.clip",
        "application/vnd.ds-editor-lite.notewithparams"};
}