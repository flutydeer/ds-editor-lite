#include "Model/Utils/ParamUtils.h"
#include "UI/Views/ClipEditor/ParamEditor/ParamEditorEditMode.h"
#include "UI/Views/ClipEditor/ParamEditor/UnsupportedParameterPromptState.h"

#include <QCoreApplication>
#include <QTextStream>

#include <optional>
#include <utility>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    SingerInfo singerWithCapabilities(
        std::optional<QStringList> acousticParameters = std::nullopt,
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

    bool testSupportFollowsSynthesisPath() {
        const auto singer = singerWithCapabilities(
            QStringList{"breathiness", "voicing", "velocity"}, true, true);
        bool ok = true;
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Pitch, singer),
                     "pitch directly controls acoustic f0");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, singer),
                     "pitch model declares expressiveness support");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Breathiness, singer),
                     "declared acoustic control is supported");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Voicing, singer),
                     "second declared acoustic control is supported");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Velocity, singer),
                     "declared transition control is supported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer),
                     "missing acoustic control is unsupported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Tension, singer),
                     "second missing acoustic control is unsupported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, singer),
                     "missing mouth opening control is unsupported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Gender, singer),
                     "missing transition control is unsupported");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::ToneShift, singer),
                     "vocoder declares pitch control support");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::SpeakerMix, singer),
                     "speaker mix uses its own capability validation");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Unknown, singer),
                     "unknown selection does not show an unsupported prompt");
        return ok;
    }


    bool testVarianceBackedParameters() {
        const auto singer = singerWithCapabilities(QStringList{"mouth_opening", "velocity"});
        bool ok = true;
        ok &= expect(ParamInfo::hasOriginalParam(ParamInfo::MouthOpening),
                     "mouth opening has a variance-generated original curve");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, singer),
                     "mouth opening support follows the singer capability");
        ok &= expect(!ParamInfo::hasOriginalParam(ParamInfo::Velocity),
                     "a singer-supported parameter without a variance curve cannot be baked");

        const auto unsupportedSinger = singerWithCapabilities(QStringList{"velocity"});
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::MouthOpening, unsupportedSinger),
                     "mouth opening is unavailable when the singer does not support it");
        return ok;
    }

    bool testEditToolVisibilityFollowsParameterType() {
        bool ok = true;
        ok &= expect(isParamEditorEditModeVisible(ParamEditorEditMode::Bake,
                                                  ParamInfo::Breathiness),
                     "a variance-backed parameter shows the bake tool");
        ok &= expect(!isParamEditorEditModeVisible(ParamEditorEditMode::Bake, ParamInfo::Gender),
                     "gender does not show the bake tool");
        ok &= expect(!isParamEditorEditModeVisible(ParamEditorEditMode::Bake, ParamInfo::Velocity),
                     "velocity does not show the bake tool");
        ok &= expect(isParamEditorEditModeVisible(ParamEditorEditMode::Draw, ParamInfo::Velocity) &&
                         isParamEditorEditModeVisible(ParamEditorEditMode::Erase,
                                                      ParamInfo::Velocity) &&
                         isParamEditorEditModeVisible(ParamEditorEditMode::Anchor,
                                                      ParamInfo::Velocity),
                     "parameter-independent tools remain visible");
        return ok;
    }

    bool testUnknownCapabilitiesAreConservative() {
        SingerInfo withoutReport({"singer", "package", QVersionNumber(1, 0)}, "Legacy Singer");
        SingerInfo withoutParameters = singerWithCapabilities(QStringList{});
        withoutParameters.setCapability(SingerCapabilitySummary{});

        bool ok = true;
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutReport),
                     "missing capability report does not disable editing");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutParameters),
                     "unknown acoustic parameters do not disable editing");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, withoutParameters),
                     "unknown pitch configuration does not disable editing");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::ToneShift, withoutParameters),
                     "unknown vocoder configuration does not disable editing");
        return ok;
    }

    bool testKnownEmptyCapabilities() {
        const auto singer = singerWithCapabilities(QStringList{}, false, false);
        bool ok = true;
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer),
                     "known empty acoustic parameters disable acoustic controls");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, singer),
                     "pitch model without expressiveness disables its control");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::ToneShift, singer),
                     "vocoder without pitch control disables tone shift");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Pitch, singer),
                     "base pitch remains supported");
        return ok;
    }

    bool testIndependentCapabilitySources() {
        const auto disabled = singerWithCapabilities(
            QStringList{"expressiveness", "tone_shift"}, false, false);
        const auto enabled = singerWithCapabilities(QStringList{}, true, true);
        bool ok = true;
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, disabled),
                     "acoustic parameter tags do not enable expressiveness");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::ToneShift, disabled),
                     "acoustic parameter tags do not enable tone shift");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, enabled),
                     "pitch configuration enables expressiveness independently");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::ToneShift, enabled),
                     "vocoder configuration enables tone shift independently");
        return ok;
    }

    bool testPromptStateResetsForEveryProjectOpen() {
        UnsupportedParameterPromptState state;
        bool ok = true;
        ok &= expect(state.shouldPrompt(ParamInfo::Energy, false),
                     "first unsupported parameter prompts on project open");
        ok &= expect(state.shouldPrompt(ParamInfo::Tension, false),
                     "each unsupported parameter prompts independently");
        ok &= expect(!state.shouldPrompt(ParamInfo::Breathiness, true),
                     "supported parameter does not prompt");

        state.acknowledge(ParamInfo::Energy);
        ok &= expect(!state.shouldPrompt(ParamInfo::Energy, false),
                     "acknowledged parameter does not prompt again in the same project");
        ok &= expect(state.shouldPrompt(ParamInfo::Tension, false),
                     "acknowledging one parameter does not suppress another");

        state.resetForProject();
        ok &= expect(state.shouldPrompt(ParamInfo::Energy, false),
                     "reopening a project resets acknowledged parameters");
        state.acknowledge(ParamInfo::Energy);
        state.resetForProject();
        ok &= expect(state.shouldPrompt(ParamInfo::Energy, false),
                     "every subsequent project open resets acknowledged parameters");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testSupportFollowsSynthesisPath();
    ok &= testVarianceBackedParameters();
    ok &= testEditToolVisibilityFollowsParameterType();
    ok &= testUnknownCapabilitiesAreConservative();
    ok &= testKnownEmptyCapabilities();
    ok &= testIndependentCapabilitySources();
    ok &= testPromptStateResetsForEveryProjectOpen();
    return ok ? 0 : 1;
}
