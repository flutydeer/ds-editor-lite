#ifndef DS_EDITOR_LITE_MIDIFILEPREPARER_H
#define DS_EDITOR_LITE_MIDIFILEPREPARER_H

#include "PreparedImportItem.h"

#include <QByteArray>
#include <QStringList>

// Parse the batch and apply one set of import options to every file.
// Preparation leaves the document and undo history untouched.
class MidiFilePreparer {
public:
    // Parses all paths in input order. A parse failure or a file without any
    // note-bearing track yields a Failed item (with reason), which does not
    // block the other files.
    static QList<PreparedImportItem> prepare(const QStringList &paths);

    // Formats a Failed item for a batch summary. Non-failed items return an
    // empty string so callers can append the result conditionally.
    static QString failureMessage(const PreparedImportItem &item);

    // Detects the shared encoding from the aggregated lyrics of all
    // successfully parsed MIDI items (falls back to the default codec).
    static QByteArray detectCommonCodec(const QList<PreparedImportItem> &prepared);

    // Builds the batch import options for one parsed file: every note-bearing
    // track is selected (empty tracks are skipped), separate MIDI channels are
    // always on, and the shared tempo/time-signature flags are applied.
    static MidiImportOptions makeBatchOptions(const QByteArray &codec, bool importTempo,
                                              bool importTimeSignature, const MidiParseData &data);
};

#endif // DS_EDITOR_LITE_MIDIFILEPREPARER_H
