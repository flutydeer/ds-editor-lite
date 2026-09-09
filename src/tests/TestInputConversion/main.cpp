#include "InputConversionTests.h"

#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Tasks/InferTaskCommon.h"
#include "Modules/Inference/Utils/PitchRouting.h"

#include <map>

namespace {
    QList<InferWord> singleWord(const int tone = 0) {
        InferPhoneme phoneme;
        phoneme.token = QStringLiteral("a");
        phoneme.languageDictId = QStringLiteral("dict");
        phoneme.tone = tone;
        InferNote note;
        note.key = 60;
        note.duration = 0.5;
        note.glide = QStringLiteral("none");
        return {InferWord({phoneme}, {note})};
    }

    void speakerMappingRows() {
        QTest::addColumn<QString>("mappingMode");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("identity") << QStringLiteral("identity") << true;
        QTest::newRow("renamed") << QStringLiteral("renamed") << true;
        QTest::newRow("missing-speaker") << QStringLiteral("missing") << false;
    }

    std::map<std::string, std::string> speakerMapping(const QString &mode) {
        if (mode == QStringLiteral("identity"))
            return {};
        if (mode == QStringLiteral("renamed"))
            return {
                {"S1", "M1"},
                {"S2", "M2"}
            };
        return {
            {"S1", "M1"}
        };
    }
}

InferSpeakerMix twoSpeakerMix(const QString &first, const QString &second) {
    InferSpeakerMix mix;
    mix.fallbackSpeaker = first;
    mix.sources = {
        {first,  0, {0.6}},
        {second, 0, {0.4}}
    };
    return mix;
}

void InputConversionTests::pieceSpeakerMix_data() {
    QTest::addColumn<bool>("hasMix");
    QTest::newRow("explicit-mix") << true;
    QTest::newRow("static-fallback") << false;
}

void InputConversionTests::pieceSpeakerMix() {
    QFETCH(bool, hasMix);
    InferPiece piece(nullptr);
    piece.speaker = QStringLiteral("S1");
    if (hasMix)
        piece.speakerMix = twoSpeakerMix();
    const auto mix = InferSpeakerMixModel::effectiveSpeakerMixForPiece(piece);
    QVERIFY(mix == (hasMix ? piece.speakerMix : InferSpeakerMixModel::staticSpeakerMix("S1")));
}

void InputConversionTests::wordSpeakerMapping_data() {
    speakerMappingRows();
}

void InputConversionTests::wordSpeakerMapping() {
    QFETCH(QString, mappingMode);
    QFETCH(bool, accepted);
    QString error;
    const auto words =
        convertInputWords(singleWord(), "S2", {}, speakerMapping(mappingMode), error);
    QCOMPARE(error.isEmpty(), accepted);
    if (!accepted) {
        QVERIFY(words.empty());
        QVERIFY(error.contains(QStringLiteral("S2")));
        return;
    }
    QCOMPARE(words.size(), size_t{1});
    QCOMPARE(words.front().phones.size(), size_t{1});
    QCOMPARE(words.front().phones.front().speakers.size(), size_t{1});
    QCOMPARE(words.front().phones.front().speakers.front().name,
             mappingMode == QStringLiteral("renamed") ? std::string("M2") : std::string("S2"));
}

void InputConversionTests::frameSpeakerMapping_data() {
    speakerMappingRows();
}

void InputConversionTests::frameSpeakerMapping() {
    QFETCH(QString, mappingMode);
    QFETCH(bool, accepted);
    QString error;
    const auto speakers = convertInputSpeakers(twoSpeakerMix(), speakerMapping(mappingMode), error);
    QCOMPARE(error.isEmpty(), accepted);
    if (!accepted) {
        QVERIFY(speakers.empty());
        QVERIFY(error.contains(QStringLiteral("S2")));
        return;
    }
    QCOMPARE(speakers.size(), size_t{2});
    QCOMPARE(speakers[0].name,
             mappingMode == QStringLiteral("renamed") ? std::string("M1") : std::string("S1"));
    QCOMPARE(speakers[1].name,
             mappingMode == QStringLiteral("renamed") ? std::string("M2") : std::string("S2"));
    QCOMPARE(speakers[0].proportions, std::vector<double>{0.6});
    QCOMPARE(speakers[1].proportions, std::vector<double>{0.4});
}

void InputConversionTests::parameterRetake_data() {
    QTest::addColumn<bool>("hasRetake");
    QTest::newRow("bounded-retake") << true;
    QTest::newRow("whole-parameter") << false;
}

void InputConversionTests::parameterRetake() {
    QFETCH(bool, hasRetake);
    InferParam parameter;
    parameter.tag = QStringLiteral("pitch");
    parameter.interval = 0.01;
    parameter.values = {0.5, 0.6};
    if (hasRetake) {
        parameter.retake.start = 1.0;
        parameter.retake.end = 2.5;
    }
    const auto converted = convertInputParams({parameter});
    QCOMPARE(converted.size(), size_t{1});
    QCOMPARE(converted.front().values, (std::vector<double>{0.5, 0.6}));
    QCOMPARE(converted.front().interval, 0.01);
    QCOMPARE(converted.front().retake.has_value(), hasRetake);
    if (hasRetake) {
        QCOMPARE(converted.front().retake->start, 1.0);
        QCOMPARE(converted.front().retake->end, 2.5);
    }
}

void InputConversionTests::vocoderPitchStaysHostOnlyAndAffectsCacheKey() {
    InferParam acousticPitch;
    acousticPitch.tag = QStringLiteral("pitch");
    acousticPitch.values = {72.0};
    InferParam vocoderPitch;
    vocoderPitch.tag = QString::fromLatin1(PitchRouting::VocoderPitchTag);
    vocoderPitch.values = {60.0};
    const auto converted = convertInputParams({acousticPitch, vocoderPitch});
    QCOMPARE(converted.size(), size_t{1});
    QVERIFY(converted.front().tag == srt::svs::Api::Common::L1::Tags::Pitch);
    QCOMPARE(converted.front().values, std::vector<double>{72.0});
    GenericInferModel first;
    first.params = {acousticPitch, vocoderPitch};
    auto second = first;
    second.params[1].values = {61.0};
    QVERIFY(first.hashData() != second.hashData());
}

void InputConversionTests::phonemeTone_data() {
    QTest::addColumn<int>("tone");
    QTest::newRow("pitched") << 60;
    QTest::newRow("unspecified") << 0;
}

void InputConversionTests::phonemeTone() {
    QFETCH(int, tone);
    QString error;
    const auto words = convertInputWords(singleWord(tone), "S1", {}, {}, error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(words.size(), size_t{1});
    QCOMPARE(words.front().phones.size(), size_t{1});
    QCOMPARE(words.front().phones.front().tone, tone);
}

void InputConversionTests::phonemeSpeakerMixUsesMappedWeights() {
    QString error;
    const auto words = convertInputWords(singleWord(), "S1", twoSpeakerMix(),
                                         {
                                             {"S1", "M1"},
                                             {"S2", "M2"}
    },
                                         error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(words.size(), size_t{1});
    QCOMPARE(words.front().phones.size(), size_t{1});
    const auto &speakers = words.front().phones.front().speakers;
    QCOMPARE(speakers.size(), size_t{2});
    QCOMPARE(speakers[0].name, std::string("M1"));
    QCOMPARE(speakers[0].proportion, 0.6);
    QCOMPARE(speakers[1].name, std::string("M2"));
    QCOMPARE(speakers[1].proportion, 0.4);
}

QTEST_GUILESS_MAIN(InputConversionTests)
