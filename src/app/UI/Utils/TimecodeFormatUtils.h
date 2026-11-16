#ifndef TIMECODEFORMATUTILS_H
#define TIMECODEFORMATUTILS_H

#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QString>
#include <QStringView>

class Timeline;

// Transport timecode text shared by the title bar field and its popup. Both
// surfaces must accept and produce exactly the same strings, so the mapping
// between a tick and every supported readout lives here instead of being
// duplicated per widget.
namespace TimecodeFormatUtils {

    // Readout of `tick` in the requested format. Negative positions clamp to
    // zero, matching the ruler and the rest of the transport.
    [[nodiscard]] QString formatPosition(const Timeline &timeline, int tick,
                                         AppearanceOption::TimecodeFormat format);

    // Parses user input for the requested format into a project tick. The
    // combined format accepts both halves on their own: text holding a decimal
    // separator is read on the seconds scale, everything else as bar:beat:tick.
    [[nodiscard]] bool parsePosition(const Timeline &timeline, QStringView text,
                                     AppearanceOption::TimecodeFormat format, int *tick);

} // namespace TimecodeFormatUtils

#endif // TIMECODEFORMATUTILS_H
