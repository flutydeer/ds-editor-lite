#ifndef TALCS_DSPXINFERENCEPIECECONTEXT_H
#define TALCS_DSPXINFERENCEPIECECONTEXT_H

#include <QObject>

namespace talcs {

    class DspxSingingClipInferenceContext;

    class DspxInferencePieceContextPrivate;

    class DspxInferencePieceContext : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(DspxInferencePieceContext)
        friend class DspxSingingClipInferenceContext;
    public:
        ~DspxInferencePieceContext() override;

        DspxSingingClipInferenceContext *singingClipInferenceContext() const;

        void setPos(int tick);
        int pos() const;

        void setLength(int tick);
        int length() const;

        void updatePosition();

        bool determine(const QString &audioFilePath = {});
        void reset();

        bool isDetermined() const;

    private:
        explicit DspxInferencePieceContext(DspxSingingClipInferenceContext *singingClipInferenceContext);
        QScopedPointer<DspxInferencePieceContextPrivate> d_ptr;
    };

}

#endif //TALCS_DSPXINFERENCEPIECECONTEXT_H
