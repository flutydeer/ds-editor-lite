#ifndef DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H
#define DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H

namespace SingingClipSlicerGlobal {
    // Maximum available lead-in for a slice, in milliseconds.
    constexpr double headerAvailableLengthMax = 1000;

    // Base SP padding, in milliseconds.
    constexpr double padBaseLength = 100;

    // Additional SP lead-in padding per pre-onset phoneme, in milliseconds.
    constexpr double padUnitAdditionalLength = 100;
}

#endif //DS_EDITOR_LITE_SINGINGCLIPSLICERGLOBAL_H
