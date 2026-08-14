#ifndef DSSP_LANGUAGE_H
#define DSSP_LANGUAGE_H

#include "DsspApi.h"

namespace DsspLanguage {

    DsspApi::Result pronunciation(const QJsonObject &body);
    DsspApi::Result phoneme(const QJsonObject &body);

} // namespace DsspLanguage

#endif // DSSP_LANGUAGE_H
