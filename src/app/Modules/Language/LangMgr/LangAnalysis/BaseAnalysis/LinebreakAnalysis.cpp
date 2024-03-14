#include "LinebreakAnalysis.h"

namespace LangMgr {
    bool LinebreakAnalysis::contains(const QChar &c) const {
        return c == QChar::LineFeed || c == QChar::LineSeparator || c == QChar::ParagraphSeparator;
    }
} // LangMgr