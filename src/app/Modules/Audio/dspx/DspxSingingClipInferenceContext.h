#ifndef TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_H
#define TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_H
#include <QObject>

namespace talcs {

    class PositionableMixerAudioSource;
    class FutureAudioSourceClipSeries;

    class DspxTrackInferenceContext;
    class DspxInferencePieceContext;

    class DspxSingingClipInferenceContextPrivate;

    class DspxSingingClipInferenceContext : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(DspxSingingClipInferenceContext);
        friend class DspxTrackInferenceContext;
        friend class DspxInferencePieceContext;

    public:
        ~DspxSingingClipInferenceContext() override;

        DspxTrackInferenceContext *trackInferenceContext() const;

        PositionableMixerAudioSource *controlMixer() const;

        void setStart(int tick);
        int start() const;

        void setClipStart(int tick);
        int clipStart() const;

        void setClipLen(int tick);
        int clipLen() const;

        void updatePosition();

        void setData(const QVariant &data);
        QVariant data() const;

        DspxInferencePieceContext *addInferencePiece(int id);
        void removeInferencePiece(int id);
        QList<DspxInferencePieceContext *> inferencePieces() const;

    private:
        explicit DspxSingingClipInferenceContext(DspxTrackInferenceContext *trackInferenceContext);
        QScopedPointer<DspxSingingClipInferenceContextPrivate> d_ptr;
    };

}

#endif // TALCS_DSPXSINGINGCLIPINFERENCECONTEXT_H
