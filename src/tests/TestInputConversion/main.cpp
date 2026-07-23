// Regression tests for v3 lite-side fixes.
// Covers: B-01 Duration speaker mix, B-02 dedup, B-04 mapSpeakerName,
//         B-05 retake fill, B-12 tone passthrough.
// Reference: docs/synthrt-refactor-v3/07-test-matrix.md §3.1, §4.1-4.3

#include "Model/AppModel/InferPiece.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Model/AppModel/InferSpeakerMix.h"
#include "Modules/Inference/Tasks/InferTaskCommon.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>
#include <map>
#include <string>

namespace {
    namespace Co = srt::svs::Api::Common::L1;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    InferSpeakerMix buildMix(const QString &s1, double w1, const QString &s2, double w2) {
        InferSpeakerMix mix;
        mix.fallbackSpeaker = s1;
        InferSpeakerMixSource src1;
        src1.speaker = s1;
        src1.interval = 0;
        src1.proportions = {w1};
        InferSpeakerMixSource src2;
        src2.speaker = s2;
        src2.interval = 0;
        src2.proportions = {w2};
        mix.sources = {src1, src2};
        return mix;
    }

    InferPhoneme makePhone(const int tone) {
        InferPhoneme phone;
        phone.token = "a";
        phone.languageDictId = "dict";
        phone.is_onset = true;
        phone.start = 0;
        phone.tone = tone;
        return phone;
    }

    InferNote makeNote() {
        InferNote note;
        note.key = 60;
        note.cents = 0;
        note.duration = 0.5;
        note.is_rest = false;
        note.glide = "none";
        return note;
    }

    QList<InferWord> makeSingleWord(const int tone) {
        return {InferWord({makePhone(tone)}, {makeNote()})};
    }

    // B-01: Duration speaker mix enablement via effectiveSpeakerMixForPiece.
    // After the v3 fix, Duration uses the same speaker-mix path as other stages,
    // so effectiveSpeakerMixForPiece must return the piece's non-empty mix
    // (rather than forcing a single static speaker).
    // Note: shouldCheckSpeakerMixSignature (InferenceApplyGate.cpp) is a private
    // free function in an anonymous namespace and cannot be unit-tested directly.
    // Its post-fix behaviour -- returning true solely based on
    // !context.speakerMixSignature.isEmpty() with no Duration special-case --
    // is exercised end-to-end through effectiveSpeakerMixForPiece being called
    // uniformly for all stages including Duration.
    bool testEffectiveSpeakerMixForPieceNonEmpty() {
        bool ok = true;
        InferPiece piece(nullptr);
        piece.speaker = "S1";
        piece.speakerMix = buildMix("S1", 0.6, "S2", 0.4);

        const auto mix = InferSpeakerMixModel::effectiveSpeakerMixForPiece(piece);
        ok &= expect(!mix.isEmpty(), "B-01: piece mix returned non-empty");
        ok &= expect(mix.fallbackSpeaker == "S1", "B-01: fallbackSpeaker is S1");
        ok &= expect(mix.sources.size() == 2, "B-01: two sources");
        ok &= expect(mix.sources.at(0).speaker == "S1" && mix.sources.at(1).speaker == "S2",
                     "B-01: source speakers S1,S2");
        ok &= expect(mix == piece.speakerMix, "B-01: returned mix equals piece mix");
        return ok;
    }

    // B-02: dedup - piece.speakerMix empty -> staticSpeakerMix(piece.speaker)
    bool testEffectiveSpeakerMixForPieceEmpty() {
        bool ok = true;
        InferPiece piece(nullptr);
        piece.speaker = "S1";
        piece.speakerMix = {};

        const auto mix = InferSpeakerMixModel::effectiveSpeakerMixForPiece(piece);
        const auto expected = InferSpeakerMixModel::staticSpeakerMix("S1");
        ok &= expect(mix == expected, "B-02: empty mix falls back to static");
        ok &= expect(mix.fallbackSpeaker == "S1", "B-02: fallback is piece.speaker");
        ok &= expect(mix.sources.size() == 1, "B-02: single source");
        ok &= expect(mix.sources.at(0).speaker == "S1", "B-02: source is piece.speaker");
        return ok;
    }

    // B-04: mapSpeakerName error - speaker not in mapping
    bool testConvertInputWordsSpeakerNotFound() {
        bool ok = true;
        const auto words = makeSingleWord(0);
        const std::map<std::string, std::string> mapping{{"S1", "M1"}};

        QString error;
        const auto result = convertInputWords(words, "S2", {}, mapping, error);
        ok &= expect(!error.isEmpty(), "B-04: error set when speaker not in mapping");
        ok &= expect(error.contains("S2"), "B-04: error mentions the missing speaker");
        ok &= expect(result.empty(), "B-04: empty vector returned on mapping error");
        return ok;
    }

    // B-04: mapSpeakerName identity - empty mapping preserves speaker name
    bool testConvertInputWordsEmptyMapping() {
        bool ok = true;
        const auto words = makeSingleWord(0);
        const std::map<std::string, std::string> mapping;

        QString error;
        const auto result = convertInputWords(words, "S1", {}, mapping, error);
        ok &= expect(error.isEmpty(), "B-04: no error with empty mapping");
        ok &= expect(result.size() == 1, "B-04: one word returned with empty mapping");
        ok &= expect(result.at(0).phones.at(0).speakers.at(0).name == "S1",
                     "B-04: identity mapping preserves speaker name");
        return ok;
    }

    // B-04: convertInputSpeakers error - same behaviour as convertInputWords
    bool testConvertInputSpeakersNotFound() {
        bool ok = true;
        const auto mix = buildMix("S1", 0.6, "S2", 0.4);
        const std::map<std::string, std::string> mapping{{"S1", "M1"}};

        QString error;
        const auto result = convertInputSpeakers(mix, mapping, error);
        ok &= expect(!error.isEmpty(), "B-04: speakers error set when speaker not in mapping");
        ok &= expect(result.empty(), "B-04: empty speakers vector on mapping error");
        return ok;
    }

    // B-04: convertInputSpeakers with empty mapping - identity
    bool testConvertInputSpeakersEmptyMapping() {
        bool ok = true;
        const auto mix = buildMix("S1", 0.6, "S2", 0.4);
        const std::map<std::string, std::string> mapping;

        QString error;
        const auto result = convertInputSpeakers(mix, mapping, error);
        ok &= expect(error.isEmpty(), "B-04: no speakers error with empty mapping");
        ok &= expect(result.size() == 2, "B-04: two speakers returned");
        ok &= expect(result.at(0).name == "S1" && result.at(1).name == "S2",
                     "B-04: identity mapping preserves speaker names");
        return ok;
    }

    // B-05: retake fill - valid range {1.0, 2.5}
    bool testConvertInputParamsRetakeSet() {
        bool ok = true;
        InferParam param;
        param.tag = "pitch";
        param.dynamic = false;
        param.interval = 0.01;
        param.values = {0.5, 0.6};
        param.retake.start = 1.0;
        param.retake.end = 2.5;

        const auto result = convertInputParams({param});
        ok &= expect(result.size() == 1, "B-05: one param returned");
        ok &= expect(result.at(0).retake.has_value(), "B-05: retake has value");
        ok &= expect(result.at(0).retake->start == 1.0, "B-05: retake start=1.0");
        ok &= expect(result.at(0).retake->end == 2.5, "B-05: retake end=2.5");
        return ok;
    }

    // B-05: retake empty - default {0, 0} yields nullopt
    bool testConvertInputParamsRetakeEmpty() {
        bool ok = true;
        InferParam param;
        param.tag = "pitch";
        param.dynamic = false;
        param.interval = 0.01;
        param.values = {0.5};
        param.retake.start = 0;
        param.retake.end = 0;

        const auto result = convertInputParams({param});
        ok &= expect(result.size() == 1, "B-05: one param returned");
        ok &= expect(!result.at(0).retake.has_value(), "B-05: retake nullopt when end<=start");
        return ok;
    }

    // B-12: tone passthrough - tone=60 and tone=0
    bool testConvertInputWordsTonePassthrough() {
        bool ok = true;
        const std::map<std::string, std::string> mapping;

        // tone = 60
        {
            QString error;
            const auto result = convertInputWords(makeSingleWord(60), "S1", {}, mapping, error);
            ok &= expect(error.isEmpty(), "B-12: no error for tone=60");
            ok &= expect(result.size() == 1, "B-12: one word for tone=60");
            ok &= expect(result.at(0).phones.at(0).tone == 60, "B-12: tone==60 passed through");
        }
        // tone = 0 (default)
        {
            QString error;
            const auto result = convertInputWords(makeSingleWord(0), "S1", {}, mapping, error);
            ok &= expect(error.isEmpty(), "B-12: no error for tone=0");
            ok &= expect(result.size() == 1, "B-12: one word for tone=0");
            ok &= expect(result.at(0).phones.at(0).tone == 0, "B-12: tone==0 passed through");
        }
        return ok;
    }
}

bool testConvertInputWordsSpeakerMix() {
    bool ok = true;
    const auto mix = buildMix("S1", 0.6, "S2", 0.4);
    std::map<std::string, std::string> mapping; // empty = identity
    QString error;
    const auto result = convertInputWords(makeSingleWord(60), "S1", mix, mapping, error);
    ok &= expect(error.isEmpty(), "B-01: no error with valid mix");
    ok &= expect(result.size() == 1, "B-01: one word");
    const auto &phones = result.at(0).phones;
    ok &= expect(phones.size() == 1, "B-01: one phone");
    const auto &speakers = phones.at(0).speakers;
    ok &= expect(speakers.size() == 2, "B-01: two speakers in mix");
    ok &= expect(speakers.at(0).name == "S1", "B-01: first speaker S1");
    ok &= expect(std::abs(speakers.at(0).proportion - 0.6) < 1e-9, "B-01: S1 proportion 0.6");
    ok &= expect(speakers.at(1).name == "S2", "B-01: second speaker S2");
    ok &= expect(std::abs(speakers.at(1).proportion - 0.4) < 1e-9, "B-01: S2 proportion 0.4");
    return ok;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= testEffectiveSpeakerMixForPieceNonEmpty();
    ok &= testEffectiveSpeakerMixForPieceEmpty();
    ok &= testConvertInputWordsSpeakerNotFound();
    ok &= testConvertInputWordsEmptyMapping();
    ok &= testConvertInputSpeakersNotFound();
    ok &= testConvertInputSpeakersEmptyMapping();
    ok &= testConvertInputParamsRetakeSet();
    ok &= testConvertInputParamsRetakeEmpty();
    ok &= testConvertInputWordsTonePassthrough();
    ok &= testConvertInputWordsSpeakerMix();

    if (!ok)
        return 1;

    QTextStream(stdout) << "TestInputConversion passed" << Qt::endl;
    return 0;
}
