// Stubs for InferPiece and its transitive dependencies.
// These allow effectiveSpeakerMixForPiece to be tested without linking
// the full AppModel / SingingClip / Note dependency chain.

#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/Core/IdGenerator.h>

#include "Model/AppOptions/AppOptions.h"

// IdGenerator singleton stub (avoids pulling in AppContext / full singleton impl)
IdGenerator *IdGenerator::instance() {
    static IdGenerator obj;
    return &obj;
}

// AppOptions stubs: InferTaskCommon.cpp defines InferRunSerializationGuard, whose
// constructor reads appOptions->inference()->executionProvider. Only the InferXxxTask
// translation units construct that guard and this test links none of them, so these
// definitions exist to satisfy the linker and are never called. Defining AppOptions for
// real would pull in every Option class plus AppContext.
AppOptions *AppOptions::instance() {
    return nullptr;
}

InferenceOption *AppOptions::inference() {
    return nullptr;
}

// InferPiece stubs: only the virtual clipId (vtable entry) and the constructor
// are needed; the remaining methods are never invoked by the conversion tests.
// NOTE: pass nullptr to QObject (not clip) to avoid requiring the full
// SingingClip definition (which would pull in a deep dependency chain).
InferPiece::InferPiece(SingingClip *clip) : QObject(nullptr), clip(clip) {
}

int InferPiece::clipId() const {
    return 0;
}

// Curve virtual method stubs (emit Curve vtable + implicit destructor)
void Curve::setLocalStart(int) {
}

int Curve::localEndTick() const {
    return 0;
}

bool Curve::isOverlappedWith(Curve *) const {
    return false;
}

std::tuple<qsizetype, qsizetype> Curve::interval() const {
    return {0, 0};
}

// DrawCurve virtual override stubs (emit DrawCurve vtable + implicit destructor)
void DrawCurve::setLocalStart(int) {
}

int DrawCurve::localEndTick() const {
    return 0;
}
