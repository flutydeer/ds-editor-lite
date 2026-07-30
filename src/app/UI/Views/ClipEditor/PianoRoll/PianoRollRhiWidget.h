#ifndef PIANOROLLRHIWIDGET_H
#define PIANOROLLRHIWIDGET_H

#include "Interface/EditorViewState.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/EditorRhiWidget.h"

#include <lite/History/HistoryFocus.h>

#include <memory>

class QEvent;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;
class SingingClip;

class PianoRollRhiWidget final : public EditorRhiWidget {
    Q_OBJECT
    Q_PROPERTY(int noteFontPixelSize READ noteFontPixelSize WRITE setNoteFontPixelSize)
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor octaveDividerColor READ octaveDividerColor WRITE setOctaveDividerColor)
    Q_PROPERTY(QColor noteSelectedBorderColor READ noteSelectedBorderColor WRITE
                   setNoteSelectedBorderColor)
    Q_PROPERTY(
        QColor pronunciationTextColor READ pronunciationTextColor WRITE setPronunciationTextColor)
    Q_PROPERTY(
        QColor clipRangeOverlayColor READ clipRangeOverlayColor WRITE setClipRangeOverlayColor)
    Q_PROPERTY(QColor paramOriginalCurveColor READ paramOriginalCurveColor WRITE
                   setParamOriginalCurveColor)
    Q_PROPERTY(
        QColor paramEditedCurveColor READ paramEditedCurveColor WRITE setParamEditedCurveColor)
    Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    Q_PROPERTY(QColor beatLineColor READ beatLineColor WRITE setBeatLineColor)
    Q_PROPERTY(QColor commonLineColor READ commonLineColor WRITE setCommonLineColor)

public:
    explicit PianoRollRhiWidget(QWidget *parent = nullptr);
    ~PianoRollRhiWidget() override;

    void setDataContext(SingingClip *clip);
    void setTrackColorIndex(int index);

    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;
    [[nodiscard]] double centerKeyIndex() const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] double scaleY() const;
    [[nodiscard]] int horizontalBarValue() const;
    [[nodiscard]] PianoRollViewState viewState() const;
    bool centerAt(double tick, double keyIndex);
    bool setViewScale(double horizontalScale, double verticalScale);
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus, bool animated = true);
    void setEditMode(ClipEditorGlobal::PianoRollEditMode mode);

public slots:
    void onWheelHorScale(QWheelEvent *event);
    void onWheelVerScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);
    void onWheelVerScroll(QWheelEvent *event);
    void setHorizontalBarValue(int value);
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);

signals:
    void timeRangeChanged(double startTick, double endTick);
    void keyRangeChanged(double topKeyIndex, double bottomKeyIndex);
    void scaleChanged(double horizontal, double vertical);
    void horizontalBarValueChanged(int value);
    void keyHovered(int keyIndex);
    void keyHoverCleared();
    void backendUnavailable();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void onRhiReady() override;
    void onDevicePixelRatioChanged() override;

private:
    class Private;
    std::unique_ptr<Private> d;

    void notifyViewportChanged();
    void notifyBackendUnavailable();

    int noteFontPixelSize() const;
    void setNoteFontPixelSize(int size);
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &color);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &color);
    QColor octaveDividerColor() const;
    void setOctaveDividerColor(const QColor &color);
    QColor noteSelectedBorderColor() const;
    void setNoteSelectedBorderColor(const QColor &color);
    QColor pronunciationTextColor() const;
    void setPronunciationTextColor(const QColor &color);
    QColor clipRangeOverlayColor() const;
    void setClipRangeOverlayColor(const QColor &color);
    QColor paramOriginalCurveColor() const;
    void setParamOriginalCurveColor(const QColor &color);
    QColor paramEditedCurveColor() const;
    void setParamEditedCurveColor(const QColor &color);
    QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);
};

#endif // PIANOROLLRHIWIDGET_H
