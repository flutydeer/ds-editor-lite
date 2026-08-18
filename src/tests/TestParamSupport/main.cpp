#include "Model/Utils/ParamUtils.h"

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
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok &= testKnownCapabilities();
    ok &= testUnknownCapabilitiesAreConservative();
    ok &= testKnownEmptyCapabilities();
    return ok ? 0 : 1;
}
