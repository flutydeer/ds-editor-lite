#include "EnglishAnalysis.h"

namespace LangMgr {
    bool isLetter(const QChar &c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool EnglishAnalysis::contains(const QChar &c) const {
        return isLetter(c);
    }

    bool EnglishAnalysis::contains(const QString &input) const {
        for (const QChar &ch : input) {
            if (!isLetter(ch)) {
                return false;
            }
        }
        return true;
    }
} // LangMgr