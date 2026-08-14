#ifndef DSSP_SYNTHESIS_H
#define DSSP_SYNTHESIS_H

#include "DsspApi.h"

namespace DsspSynthesis {

    DsspApi::Result duration(const QJsonObject &body);
    DsspApi::Result parameter(const QJsonObject &body);
    DsspApi::Result audio(const QJsonObject &body);

} // namespace DsspSynthesis

#endif // DSSP_SYNTHESIS_H
