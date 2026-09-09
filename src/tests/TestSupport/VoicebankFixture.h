#pragma once

#include <QDir>
#include <QString>

namespace TestSupport {
    inline bool usingBundledVoicebank() {
        return qEnvironmentVariableIsEmpty("DSEL_TEST_VOICEBANK_ROOT");
    }

    inline QString voicebankRoot() {
        return QDir::cleanPath(usingBundledVoicebank()
                                   ? QString::fromUtf8(LITE_TEST_VOICEBANK_ROOT)
                                   : qEnvironmentVariable("DSEL_TEST_VOICEBANK_ROOT"));
    }

    inline QString fixtureSingerId() {
        return usingBundledVoicebank() ? QStringLiteral("fixture")
                                       : qEnvironmentVariable("DSEL_TEST_SINGER_ID");
    }

    inline QString fixtureLanguage() {
        return usingBundledVoicebank() ? QStringLiteral("cmn")
                                       : qEnvironmentVariable("DSEL_TEST_LANGUAGE");
    }

    inline QString fixtureLyric() {
        return usingBundledVoicebank() ? QStringLiteral("la")
                                       : qEnvironmentVariable("DSEL_TEST_LYRIC");
    }
}
