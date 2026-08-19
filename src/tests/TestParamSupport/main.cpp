#include "Model/Utils/ParamUtils.h"
#include "UI/Views/ClipEditor/ParamEditor/UnsupportedParameterPromptState.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    SingerInfo singerWithParameters(const QStringList &parameters) {
        SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Test Singer");
        SingerCapabilitySummary capability;
        capability.acousticParameters = parameters;
        singer.setCapability(capability);
        return singer;
    }

    bool testKnownCapabilities() {
        const auto singer = singerWithParameters({"breathiness", "voicing", "velocity"});
        bool ok = true;
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Breathiness, singer),
                     "declared parameter is supported");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Velocity, singer),
                     "second declared parameter is supported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer),
                     "missing parameter is unsupported");
        ok &= expect(!paramUtils->isSupportedBySinger(ParamInfo::ToneShift, singer),
                     "missing relative parameter is unsupported");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Expressiveness, singer),
                     "non-acoustic parameter remains available");
        return ok;
    }

    bool testUnknownCapabilitiesAreConservative() {
        SingerInfo withoutReport({"singer", "package", QVersionNumber(1, 0)}, "Legacy Singer");
        SingerInfo withoutParameters = singerWithParameters({});
        withoutParameters.setCapability(SingerCapabilitySummary{});

        bool ok = true;
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutReport),
                     "missing capability report does not disable editing");
        ok &= expect(paramUtils->isSupportedBySinger(ParamInfo::Energy, withoutParameters),
                     "unknown acoustic parameters do not disable editing");
        return ok;
    }

    bool testKnownEmptyCapabilities() {
        const auto singer = singerWithParameters({});
        return expect(!paramUtils->isSupportedBySinger(ParamInfo::Energy, singer),
                      "known empty acoustic parameters disable acoustic controls");
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
    ok &= testKnownCapabilities();
    ok &= testUnknownCapabilitiesAreConservative();
    ok &= testKnownEmptyCapabilities();
    ok &= testPromptStateResetsForEveryProjectOpen();
    return ok ? 0 : 1;
}
