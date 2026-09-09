#include "tst_lyrics.h"

#include "Modules/FillLyric/Utils/LrcTools/LrcDecoder.h"
#include "Modules/FillLyric/Utils/SplitLyric.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {
    bool writeText(const QString &path, const QByteArray &text) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(text) == text.size();
    }

    QList<QStringList> lyrics(const QList<QList<LangNote>> &lines) {
        QList<QStringList> result;
        for (const auto &line : lines) {
            QStringList words;
            for (const auto &note : line)
                words.append(note.lyric);
            result.append(words);
        }
        return result;
    }
}

void LyricsTests::lrcTimestamps_data() {
    QTest::addColumn<QByteArray>("timestamp");
    QTest::addColumn<qint64>("milliseconds");
    QTest::newRow("whole-seconds") << QByteArray("01:02") << qint64(62000);
    QTest::newRow("tenths") << QByteArray("00:02.5") << qint64(2500);
    QTest::newRow("hundredths") << QByteArray("00:02.05") << qint64(2050);
    QTest::newRow("milliseconds") << QByteArray("00:02.005") << qint64(2005);
}

void LyricsTests::lrcTimestamps() {
    QFETCH(QByteArray, timestamp);
    QFETCH(qint64, milliseconds);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("lyrics.lrc");
    QVERIFY(writeText(path, '[' + timestamp + "]la"));
    LrcTools::LrcDecoder decoder;
    QVERIFY2(decoder.decode(path), qPrintable(decoder.lastError()));
    const auto packet = decoder.readPacket();
    QCOMPARE(packet.pts, milliseconds);
    QCOMPARE(packet.lyric, QStringLiteral("la"));
    QCOMPARE(decoder.duration(), milliseconds);
    QVERIFY(decoder.readPacket().isEmpty());
}

void LyricsTests::lrcMetadataRepeatedLinesAndSeeking() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("多行.lrc"));
    QVERIFY(writeText(path, "[ti:Test song]\r\n[ar:Fixture]\r\n\r\n"
                            "[00:03.00]last\r\n[00:01.00][00:02.00]la\r\n"
                            "unmarked line\r\n[00:xx]invalid"));
    LrcTools::LrcDecoder decoder;
    QVERIFY2(decoder.decode(path), qPrintable(decoder.lastError()));
    QCOMPARE(decoder.get("title"), QStringLiteral("Test song"));
    QCOMPARE(decoder.get("artist"), QStringLiteral("Fixture"));
    QCOMPARE(decoder.dumpLyrics(), (QStringList{"la", "la", "last"}));
    QVERIFY(decoder.seek(1500));
    QCOMPARE(decoder.readPacket().pts, 2000);
    QVERIFY(decoder.seek(1500, LrcTools::LrcDecoder::SeekBackward));
    QCOMPARE(decoder.readPacket().pts, 1000);
    QVERIFY(!decoder.seek(4000));
    QVERIFY(decoder.readPacket().isEmpty());
}

void LyricsTests::lrcFailedReloadClearsPreviousDocument() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("lyrics.lrc");
    LrcTools::LrcDecoder decoder;
    QVERIFY(writeText(path, "[ti:Previous]\n[00:01.00]la"));
    QVERIFY(decoder.decode(path));
    QVERIFY(!decoder.decode(directory.filePath("missing.lrc")));
    QVERIFY(!decoder.lastError().isEmpty());
    QVERIFY(decoder.get("title").isEmpty());
    QCOMPARE(decoder.duration(), 0);
    QVERIFY(decoder.dumpLyrics().isEmpty());
    QVERIFY(decoder.readPacket().isEmpty());
    QVERIFY(!decoder.seek(0));
    QVERIFY(writeText(path, "[ti:Metadata only]\n"));
    QVERIFY(!decoder.decode(path));
    QVERIFY(writeText(path, "[00:02.00]new"));
    QVERIFY(decoder.decode(path));
    QVERIFY(decoder.lastError().isEmpty());
    QCOMPARE(decoder.readPacket().lyric, QStringLiteral("new"));
}

void LyricsTests::lyricSplittingModesPreserveLines() {
    using FillLyric::LyricSplitter;
    QTemporaryDir rules;
    QVERIFY(rules.isValid());
    const auto rulePath = std::filesystem::path(rules.path().toStdU16String());
    QVERIFY(FillLyric::TextTagger::init(rulePath, rulePath));
    QCOMPARE(lyrics(LyricSplitter::splitByChar(QStringLiteral("你 好\r\n\n世界\n"),
                                               {
    })),
             (QList<QStringList>{{QStringLiteral("你"), QStringLiteral("好")},
                                 {QStringLiteral("世"), QStringLiteral("界")}}));
    QCOMPARE(lyrics(LyricSplitter::splitCustom("hello,world\nla|mi",
                                               {
                                                   ",", "|"
    },
                                               {})),
             (QList<QStringList>{{"hello", "world"}, {"la", "mi"}}));
    QVERIFY(LyricSplitter::splitByChar(" \r\n", {}).isEmpty());
    QVERIFY(LyricSplitter::splitCustom(" ,|\n", {",", "|"}, {}).isEmpty());
}
