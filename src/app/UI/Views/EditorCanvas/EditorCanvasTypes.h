#ifndef EDITORCANVASTYPES_H
#define EDITORCANVASTYPES_H

#include <QColor>
#include <QFlags>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>

enum class EditorCanvasBackend {
    Legacy,
    ExperimentalRhi,
};

enum class EditorCanvasKind {
    TrackEditor,
    PianoRoll,
};

enum class EditorDirtyDomain : std::uint32_t {
    None = 0,
    Camera = 1U << 0U,
    Geometry = 1U << 1U,
    Style = 1U << 2U,
    Text = 1U << 3U,
    Waveform = 1U << 4U,
    Selection = 1U << 5U,
    Overlay = 1U << 6U,
    All = 0x7FU,
};
Q_DECLARE_FLAGS(EditorDirtyDomains, EditorDirtyDomain)
Q_DECLARE_OPERATORS_FOR_FLAGS(EditorDirtyDomains)

struct EditorViewportState {
    double centerTick = 0.0;
    double centerTrack = 0.0;
    double centerKey = 60.0;
    double horizontalScale = 1.0;
    double verticalScale = 1.0;
    double startTick = 0.0;
    double endTick = 0.0;
    double topValue = 0.0;
    double bottomValue = 0.0;
    QSize viewportSize;
    double devicePixelRatio = 1.0;
    double playbackPosition = 0.0;
    double lastPlaybackPosition = 0.0;
    QPointF animationTarget;
};

struct EditorHitResult {
    enum class Part {
        None,
        Body,
        LeftEdge,
        RightEdge,
        Curve,
        Anchor,
    };

    int objectId = -1;
    Part part = Part::None;

    [[nodiscard]] bool isValid() const {
        return objectId >= 0 && part != Part::None;
    }
};

struct EditorRenderRect {
    int objectId = -1;
    QRectF bounds;
    QColor fill;
    QColor border;
    bool selected = false;
    bool hovered = false;
    int layer = 0;
};

struct EditorRenderLine {
    QPointF start;
    QPointF end;
    QColor color;
    float width = 1.0F;
    int layer = 0;
};

enum class EditorStrokeJoin {
    Miter,
    Bevel,
    Round,
};

enum class EditorStrokeCap {
    Butt,
    Round,
};

struct EditorRenderPath {
    QVector<QPointF> points;
    QColor color;
    float width = 1.0F;
    EditorStrokeJoin join = EditorStrokeJoin::Round;
    EditorStrokeCap cap = EditorStrokeCap::Round;
    int layer = 0;
};

struct EditorRenderGlyph {
    quint32 glyphIndex = 0;
    QPointF position;
    QRectF clip;
    QColor color;
};

struct EditorRenderText {
    QString text;
    QPointF baseline;
    QColor color;
    QFont font;
    QRectF clip;
    int layer = 0;
};

struct EditorRenderSnapshot {
    EditorCanvasKind kind = EditorCanvasKind::TrackEditor;
    quint64 revision = 0;
    QSizeF logicalExtent;
    QVector<EditorRenderRect> rectangles;
    QVector<EditorRenderLine> lines;
    QVector<EditorRenderPath> paths;
    QVector<EditorRenderGlyph> glyphs;
    QVector<EditorRenderText> texts;
    QVector<int> selectedIds;
    int hoveredId = -1;
    double playbackPosition = 0.0;
    double lastPlaybackPosition = 0.0;
};

struct EditorRenderMetrics {
    quint64 resourceGeneration = 0;
    quint64 snapshotRevision = 0;
    quint64 frameNumber = 0;
    quint64 cachedFrameCount = 0;
    quint64 dynamicFrameCount = 0;
    quint64 drawCalls = 0;
    quint64 vertices = 0;
    quint64 uploadBytes = 0;
    quint64 atlasHits = 0;
    quint64 atlasMisses = 0;
    qint64 frameIntervalNanoseconds = 0;
    qint64 snapshotBuildNanoseconds = 0;
    qint64 updateToFrameSubmittedNanoseconds = 0;
    qint64 tessellationNanoseconds = 0;
    qint64 commandEncodingNanoseconds = 0;
};

using ImmutableEditorRenderSnapshot = std::shared_ptr<const EditorRenderSnapshot>;

QString editorCanvasBackendKey(EditorCanvasBackend backend);
EditorCanvasBackend editorCanvasBackendFromKey(const QString &key);

#endif // EDITORCANVASTYPES_H
