#include "HistoryFocus.h"

#include <cmath>

bool HistoryFocus::isValid() const {
    return std::isfinite(tickStart) && std::isfinite(tickEnd) && std::isfinite(valueStart) &&
           std::isfinite(valueEnd) && tickStart <= tickEnd && valueStart <= valueEnd;
}
