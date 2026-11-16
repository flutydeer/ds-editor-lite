#ifndef APPEARANCEOPTION_H
#define APPEARANCEOPTION_H

#include "Model/AppOptions/IOption.h"

#include <optional>

class AppearanceOption : public IOption {
public:
    // How the transport timecode renders the playhead position: the musical
    // bar:beat:tick readout, the seconds readout (m:ss.d), or both.
    enum class TimecodeFormat { BarBeatTick, Time, Both };

    explicit AppearanceOption() : IOption("appearance") {
    }

    void load(const QJsonObject &object) override;

#ifdef Q_OS_MAC
    bool useNativeFrame = true;
#else
    bool useNativeFrame = false;
#endif
    // Runtime-only override for screenshot capture. Serialization keeps useNativeFrame.
    std::optional<bool> nativeFrameOverride;
    [[nodiscard]] bool effectiveUseNativeFrame() const {
        return nativeFrameOverride.value_or(useNativeFrame);
    }

    // themeId is stored as an opaque persisted string; the theme system owns
    // the vocabulary and converts at the boundary. Animation is a plain on/off
    // toggle; a missing or unparseable value defaults to enabled.
    bool animationEnabled = true;
    double animationTimeScale = 1;
    QString themeId = QStringLiteral("system");
    // Empty string means the platform-default UI font (see FontManager).
    // The font setting is hot-swappable; no restart required.
    QString uiFontFamily;
    // Draw notes from other tracks in the piano roll as thin reference bars.
    bool showGhostNotes = true;
    // Show the tempo / time signature lane between the piano roll ruler and
    // the note canvas. Hidden by default.
    bool showTempoLane = false;
    bool showTimeSignatureLane = false;
    // Seconds time axis in the piano roll, below its ruler. The track editor's
    // copy is a per-window menu toggle like the tempo and time signature lanes.
    // Hidden by default, so projects keep the bar/beat-only layout.
    bool showTimeLane = false;
    // Transport timecode format; the bar:beat:tick readout stays the default.
    TimecodeFormat timecodeFormat = TimecodeFormat::BarBeatTick;

    // Stable persisted ids of TimecodeFormat. Unknown ids fall back to the
    // caller's default instead of being written back verbatim.
    [[nodiscard]] static QString timecodeFormatId(TimecodeFormat format);
    [[nodiscard]] static TimecodeFormat
        timecodeFormatFromId(const QString &id,
                             TimecodeFormat fallback = TimecodeFormat::BarBeatTick);

protected:
    void save(QJsonObject &object) override;

private:
    const QString useNativeFrameKey = "useNativeFrame";
    const QString animationEnabledKey = "animationEnabled";
    const QString animationTimeScaleKey = "animationTimeScale";
    const QString themeIdKey = "themeId";
    const QString uiFontFamilyKey = "uiFontFamily";
    const QString showGhostNotesKey = "showGhostNotes";
    const QString showTempoLaneKey = "showTempoLane";
    const QString showTimeSignatureLaneKey = "showTimeSignatureLane";
    const QString showTimeLaneKey = "showTimeLane";
    const QString timecodeFormatKey = "timecodeFormat";
};

#endif // APPEARANCEOPTION_H
