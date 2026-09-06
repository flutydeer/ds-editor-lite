#ifndef DS_EDITOR_LITE_PITCHCURVETRANSFORMCONTEXT_H
#define DS_EDITOR_LITE_PITCHCURVETRANSFORMCONTEXT_H

#include "CurveTransformSession.h"
#include "Modules/Inference/Utils/BasePitchCurve.h"

#include <QHash>
#include <QPointer>
#include <QVector>

#include <memory>
#include <optional>

class SingingClip;

namespace CurveTransform {
    class PitchContext final {
    public:
        void rebuild(SingingClip *clip);
        void invalidate();
        void clear();

        [[nodiscard]] const QList<Interval> &partitions() const;
        [[nodiscard]] std::optional<double> baselineAtTick(int localTick) const;

    private:
        struct PieceCache {
            Interval interval;
            double originMs = 0.0;
            std::shared_ptr<BasePitchCurve> curve;
        };

        struct CurveCache {
            std::vector<BasePitchCurve::InputNote> notes;
            std::shared_ptr<BasePitchCurve> curve;
        };

        QPointer<SingingClip> m_clip;
        quint64 m_clipRevision = 0;
        bool m_valid = false;
        QList<Interval> m_partitions;
        QVector<PieceCache> m_pieces;
        QHash<int, CurveCache> m_curveCache;
    };
}

#endif // DS_EDITOR_LITE_PITCHCURVETRANSFORMCONTEXT_H
