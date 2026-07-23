// Stubs for InferPiece and its transitive dependencies.
// These allow effectiveSpeakerMixForPiece to be tested without linking
// the full AppModel / SingingClip / Note dependency chain.

#include "Model/InferenceData/InferPiece.h"
#include "Model/AppModel/DrawCurve.h"
#include "Utils/IdGenerator.h"

// IdGenerator singleton stub (avoids pulling in AppContext / full singleton impl)
IdGenerator *IdGenerator::instance() {
    static IdGenerator obj;
    return &obj;
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
