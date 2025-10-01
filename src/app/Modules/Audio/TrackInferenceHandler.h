#ifndef TRACKINFERENCEHANDLER_H
#define TRACKINFERENCEHANDLER_H

#include "TrackInferenceHandler.h"
#include "Model/AppModel/InferStatus.h"
#include "dspx/DspxTrackInferenceContext.h"

#include <QObject>
#include <QHash>

namespace talcs {
    class DspxTrackContext;
    class DspxInferencePieceContext;
}

class Track;
class SingingClip;
class InferPiece;

class TrackInferenceHandler : public talcs::DspxTrackInferenceContext {
    Q_OBJECT
public:
    explicit TrackInferenceHandler(talcs::DspxTrackContext *trackContext, Track *track);
    ~TrackInferenceHandler() override;

private:
    Track *m_track;
    talcs::DspxTrackContext *m_trackContext;

    QHash<SingingClip *, talcs::DspxSingingClipInferenceContext *> m_singingClipModelDict;
    QHash<InferPiece *, talcs::DspxInferencePieceContext *> m_inferPieceModelDict;
    QHash<SingingClip *, QList<InferPiece *>> m_singingClipInferPieces;

    void handleSingingClipInserted(SingingClip *clip);
    void handleSingingClipRemoved(SingingClip *clip);

    void handleSingingClipPropertyChanged(SingingClip *clip) const;
    void handlePieceChanged(SingingClip *clip, const QList<InferPiece *> &pieces);

    void handlePieceInserted(SingingClip *clip, InferPiece *inferPiece);
    void handlePieceRemoved(SingingClip *clip, InferPiece *inferPiece);

    void handleInferPieceStatusChange(InferPiece *piece, InferStatus status) const;

    void handleTimeChanged() const;
};




#endif // TRACKINFERENCEHANDLER_H
