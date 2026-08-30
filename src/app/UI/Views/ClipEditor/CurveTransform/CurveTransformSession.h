#ifndef DS_EDITOR_LITE_CURVETRANSFORMSESSION_H
#define DS_EDITOR_LITE_CURVETRANSFORMSESSION_H

#include <QList>
#include <QVector>

#include <functional>
#include <optional>

class DrawCurve;
class ParamProperties;

namespace CurveTransform {
    constexpr int SampleStep = 5;
    constexpr double DefaultShoulderMaximumMs = 60.0;

    enum class Kind { Shape, Scale, ModulatePitch };
    enum class Phase { Idle, Selecting, Adjusting, Transforming };
    enum class Boundary { None, C, A, B, D };

    struct Interval {
        int startTick = 0;
        int endTick = -1;
    };

    struct Bounds {
        int componentStart = 0;
        int c = 0;
        int a = 0;
        int b = 0;
        int d = 0;
        int componentEnd = 0;

        [[nodiscard]] bool isValid() const;
    };

    struct Config {
        Kind kind = Kind::Shape;
        const ParamProperties *properties = nullptr;
        QList<Interval> partitions;
        std::function<double(int)> tickToMilliseconds;
        std::function<std::optional<double>(int)> pitchBaselineAtTick;
    };

    [[nodiscard]] double smoothWeight(int tick, const Bounds &bounds);
    [[nodiscard]] double factorAt(int tick, const Bounds &bounds, double factor);
    [[nodiscard]] std::optional<Interval> completeSampleInterval(int startTick, int endTick);

    class Session final {
    public:
        Session();
        ~Session();
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;

        void setSource(const QList<DrawCurve *> &original, const QList<DrawCurve *> &edited,
                       Config config);
        void clear();
        void cancel();

        [[nodiscard]] bool hasSource() const;
        [[nodiscard]] Phase phase() const;
        [[nodiscard]] const Bounds &bounds() const;
        [[nodiscard]] double factor() const;

        void beginSelection(int tick);
        bool updateSelection(int tick);
        bool finishSelection(int tick);

        bool setBoundary(Boundary boundary, int tick);
        bool beginTransform();
        void updateTransform(double verticalLogicalPixelDelta);

        [[nodiscard]] QList<DrawCurve *> buildEditedPreview() const;
        [[nodiscard]] bool hasEffectiveChange() const;

    private:
        struct Component {
            int startTick = 0;
            QVector<int> values;

            [[nodiscard]] int endTick() const;
            [[nodiscard]] int valueAt(int tick) const;
        };

        [[nodiscard]] int partitionAt(int tick) const;
        [[nodiscard]] int firstTouchedComponent(int fromTick, int toTick) const;
        bool updateSelectionCandidate(int tick);
        void initializeShoulders();
        [[nodiscard]] int defaultShoulderWidth(int boundaryTick, int availableTicks,
                                               bool left) const;
        [[nodiscard]] DrawCurve transformedCurve() const;
        [[nodiscard]] int transformedValueAt(int tick) const;
        void resetInteraction();

        Config m_config;
        QVector<Component> m_components;
        QList<DrawCurve *> m_editedSnapshot;
        Phase m_phase = Phase::Idle;
        Bounds m_bounds;
        int m_selectionStartTick = 0;
        int m_selectedComponent = -1;
        double m_factor = 1.0;
    };
}

#endif // DS_EDITOR_LITE_CURVETRANSFORMSESSION_H
