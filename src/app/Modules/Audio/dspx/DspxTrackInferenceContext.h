#ifndef TALCS_DSPXTRACKINFERENCECONTEXT_H
#define TALCS_DSPXTRACKINFERENCECONTEXT_H

#include <QObject>

namespace talcs {

    class AudioSourceClipSeries;

    class DspxSingingClipInferenceContext;

    class DspxTrackInferenceContextPrivate;

    class DspxTrackInferenceContext : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(DspxTrackInferenceContext)
        friend class DspxSingingClipInferenceContext;
    public:
        explicit DspxTrackInferenceContext(QObject *parent = nullptr);
        ~DspxTrackInferenceContext() override;

        AudioSourceClipSeries *clipSeries() const;

        enum Mode {
            Default,
            Export,
            VST,
        };
        Mode mode() const;
        void setMode(Mode mode);

        DspxSingingClipInferenceContext *addSingingClip(int id);
        void removeSingingClip(int id);

        QList<DspxSingingClipInferenceContext *> clips() const;

    private:
        QScopedPointer<DspxTrackInferenceContextPrivate> d_ptr;

    };

}

#endif //TALCS_DSPXTRACKINFERENCECONTEXT_H
