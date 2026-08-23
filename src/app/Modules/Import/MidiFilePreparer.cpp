#include "MidiFilePreparer.h"

#include <lite/ProjectConverters/MidiTextCodecConverter.h>

QList<PreparedImportItem> MidiFilePreparer::prepare(const QStringList &paths) {
    QList<PreparedImportItem> result;
    result.reserve(paths.size());
    for (const auto &path : paths) {
        PreparedImportItem item;
        item.path = path;
        auto parseData = MidiFileParser::parse(path);
        if (!parseData.valid) {
            item.kind = PreparedImportItem::Kind::Failed;
            item.errorMessage = parseData.errorMessage;
            result.append(item);
            continue;
        }
        bool hasNotes = false;
        for (const auto &info : parseData.trackInfos) {
            if (info.noteCount > 0) {
                hasNotes = true;
                break;
            }
        }
        if (!hasNotes) {
            item.kind = PreparedImportItem::Kind::Failed;
            item.errorMessage = QObject::tr("No notes in MIDI file");
            result.append(item);
            continue;
        }
        item.kind = PreparedImportItem::Kind::Midi;
        item.midi = std::move(parseData);
        result.append(item);
    }
    return result;
}

QString MidiFilePreparer::failureMessage(const PreparedImportItem &item) {
    if (item.kind != PreparedImportItem::Kind::Failed)
        return {};
    if (item.path.isEmpty())
        return item.errorMessage;
    if (item.errorMessage.isEmpty())
        return item.path;
    return item.path + QStringLiteral(" - ") + item.errorMessage;
}

QByteArray MidiFilePreparer::detectCommonCodec(const QList<PreparedImportItem> &prepared) {
    QByteArray lyrics;
    for (const auto &item : prepared) {
        if (item.kind != PreparedImportItem::Kind::Midi)
            continue;
        for (const auto &info : item.midi.trackInfos) {
            for (const auto &lyric : info.lyrics)
                lyrics.append(lyric);
        }
    }
    const auto detected = MidiTextCodecConverter::detectEncoding(lyrics);
    return detected.isEmpty() ? MidiTextCodecConverter::defaultCodec() : detected;
}

MidiImportOptions MidiFilePreparer::makeBatchOptions(const QByteArray &codec,
                                                     const bool importTempo,
                                                     const bool importTimeSignature,
                                                     const MidiParseData &data) {
    MidiImportOptions options;
    options.codec = codec;
    for (int i = 0; i < data.trackInfos.size(); ++i) {
        if (data.trackInfos.at(i).selectedByDefault)
            options.selectedTrackIndices.append(i);
    }
    options.importTempo = importTempo;
    options.importTimeSignature = importTimeSignature;
    return options;
}
