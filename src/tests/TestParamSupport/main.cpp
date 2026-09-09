#include "Model/Utils/ParamUtils.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorEditMode.h"
#include "UI/Views/ClipEditor/ParamEditor/UnsupportedParameterPromptState.h"

#include <QCoreApplication>
#include <QtTest/QTest>

#include <optional>
#include <utility>

namespace {

    SingerInfo singerWithCapabilities(std::optional<QStringList> acousticParameters = std::nullopt,
                                      std::optional<bool> pitchUsesExpressiveness = std::nullopt,
                                      std::optional<bool> vocoderPitchControllable = std::nullopt) {
        SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Test Singer");
        SingerCapabilitySummary capability;
        capability.acousticParameters = std::move(acousticParameters);
        capability.pitchUsesExpressiveness = pitchUsesExpressiveness;
        capability.vocoderPitchControllable = vocoderPitchControllable;
        singer.setCapability(capability);
        return singer;
    }

}

class ParamSupportTests final : public QObject {
    Q_OBJECT

private slots:

    void supportFollowsSynthesisPath() {
        const auto singer =
            singerWithCapabilities(QStringList{"breathiness", "voicing", "velocity"}, true, true);
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Pitch, singer)),
                 "pitch directly controls acoustic f0");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, singer)),
                 "pitch model declares expressiveness support");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Breathiness, singer)),
                 "declared acoustic control is supported");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Voicing, singer)),
                 "second declared acoustic control is supported");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Velocity, singer)),
                 "declared transition control is supported");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer)),
                 "missing acoustic control is unsupported");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Tension, singer)),
                 "second missing acoustic control is unsupported");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, singer)),
                 "missing mouth opening control is unsupported");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Gender, singer)),
                 "missing transition control is unsupported");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::ToneShift, singer)),
                 "vocoder declares pitch control support");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::SpeakerMix, singer)),
                 "speaker mix uses its own capability validation");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Unknown, singer)),
                 "unknown selection does not show an unsupported prompt");
    }

    void varianceBackedParameters() {
        const auto singer = singerWithCapabilities(QStringList{"mouth_opening", "velocity"});
        QVERIFY2((ParamInfo::hasOriginalParam(ParamInfo::MouthOpening)),
                 "mouth opening has a variance-generated original curve");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, singer)),
                 "mouth opening support follows the singer capability");
        QVERIFY2((!ParamInfo::hasOriginalParam(ParamInfo::Velocity)),
                 "a singer-supported parameter without a variance curve cannot be traced");

        const auto unsupportedSinger = singerWithCapabilities(QStringList{"velocity"});
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, unsupportedSinger)),
                 "mouth opening is unavailable when the singer does not support it");
    }

    void editToolVisibilityFollowsParameterType() {
        QVERIFY2((isParamEditorEditModeVisible(ParamEditorEditMode::Trace, ParamInfo::Breathiness)),
                 "a variance-backed parameter shows the trace tool");
        QVERIFY2((!isParamEditorEditModeVisible(ParamEditorEditMode::Trace, ParamInfo::Gender)),
                 "gender does not show the trace tool");
        QVERIFY2((!isParamEditorEditModeVisible(ParamEditorEditMode::Trace, ParamInfo::Velocity)),
                 "velocity does not show the trace tool");
        QVERIFY2(
            (isParamEditorEditModeVisible(ParamEditorEditMode::Shape, ParamInfo::Breathiness) &&
             isParamEditorEditModeVisible(ParamEditorEditMode::Scale, ParamInfo::MouthOpening)),
            "transformable parameters show curve transform tools");
        QVERIFY2((!isParamEditorEditModeVisible(ParamEditorEditMode::Shape, ParamInfo::Gender) &&
                  !isParamEditorEditModeVisible(ParamEditorEditMode::Scale, ParamInfo::Velocity)),
                 "offset parameters do not show curve transform tools");
        QVERIFY2((isParamEditorEditModeVisible(ParamEditorEditMode::Draw, ParamInfo::Velocity) &&
                  isParamEditorEditModeVisible(ParamEditorEditMode::Erase, ParamInfo::Velocity) &&
                  isParamEditorEditModeVisible(ParamEditorEditMode::Anchor, ParamInfo::Velocity)),
                 "parameter-independent tools remain visible");
    }

    void unknownCapabilitiesAreConservative() {
        SingerInfo withoutReport({"singer", "package", QVersionNumber(1, 0)}, "Legacy Singer");
        SingerInfo withoutParameters = singerWithCapabilities(QStringList{});
        withoutParameters.setCapability(SingerCapabilitySummary{});

        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutReport)),
                 "missing capability report does not disable editing");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutParameters)),
                 "unknown acoustic parameters do not disable editing");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, withoutParameters)),
                 "unknown pitch configuration does not disable editing");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::ToneShift, withoutParameters)),
                 "unknown vocoder configuration does not disable editing");
    }

    void knownEmptyCapabilities() {
        const auto singer = singerWithCapabilities(QStringList{}, false, false);
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer)),
                 "known empty acoustic parameters disable acoustic controls");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, singer)),
                 "pitch model without expressiveness disables its control");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::ToneShift, singer)),
                 "vocoder without pitch control disables tone shift");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Pitch, singer)),
                 "base pitch remains supported");
    }

    void independentCapabilitySources() {
        const auto disabled =
            singerWithCapabilities(QStringList{"expressiveness", "tone_shift"}, false, false);
        const auto enabled = singerWithCapabilities(QStringList{}, true, true);
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, disabled)),
                 "acoustic parameter tags do not enable expressiveness");
        QVERIFY2((!paramUtils->isSupportedBySinger(ParamInfo::ToneShift, disabled)),
                 "acoustic parameter tags do not enable tone shift");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, enabled)),
                 "pitch configuration enables expressiveness independently");
        QVERIFY2((paramUtils->isSupportedBySinger(ParamInfo::ToneShift, enabled)),
                 "vocoder configuration enables tone shift independently");
    }

    void promptStateResetsForEveryProjectOpen() {
        UnsupportedParameterPromptState state;
        QVERIFY2((state.shouldPrompt(ParamInfo::Energy, false)),
                 "first unsupported parameter prompts on project open");
        QVERIFY2((state.shouldPrompt(ParamInfo::Tension, false)),
                 "each unsupported parameter prompts independently");
        QVERIFY2((!state.shouldPrompt(ParamInfo::Breathiness, true)),
                 "supported parameter does not prompt");

        state.acknowledge(ParamInfo::Energy);
        QVERIFY2((!state.shouldPrompt(ParamInfo::Energy, false)),
                 "acknowledged parameter does not prompt again in the same project");
        QVERIFY2((state.shouldPrompt(ParamInfo::Tension, false)),
                 "acknowledging one parameter does not suppress another");

        state.resetForProject();
        QVERIFY2((state.shouldPrompt(ParamInfo::Energy, false)),
                 "reopening a project resets acknowledged parameters");
        state.acknowledge(ParamInfo::Energy);
        state.resetForProject();
        QVERIFY2((state.shouldPrompt(ParamInfo::Energy, false)),
                 "every subsequent project open resets acknowledged parameters");
    }
};

QTEST_GUILESS_MAIN(ParamSupportTests)
#include "main.moc"
