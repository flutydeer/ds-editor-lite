#ifndef DSSP_EXTRACTION_H
#define DSSP_EXTRACTION_H

#include "DsspApi.h"

namespace DsspExtraction {

    DsspApi::Result extractorList();
    DsspApi::Result noteExtractor(const QString &extractorId);
    DsspApi::Result tempoExtractor(const QString &extractorId);
    DsspApi::Result pitchExtractor(const QString &extractorId);
    DsspApi::Result extractNote(const QJsonObject &body);
    DsspApi::Result extractTempo(const QJsonObject &body);
    DsspApi::Result extractPitch(const QJsonObject &body);

} // namespace DsspExtraction

#endif // DSSP_EXTRACTION_H
