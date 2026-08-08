#ifndef DS_EDITOR_LITE_USERINPUT_H
#define DS_EDITOR_LITE_USERINPUT_H

#include <QByteArray>
#include <QList>

// Basic UserInput DTOs: the configuration pages produce these, sessions
// consume them. Sessions never read widget state directly.

struct TextEncodingInput {
    QByteArray codec;
};

struct TrackSelectionInput {
    QList<int> selectedTrackIndices;
};

struct ChannelSeparationInput {
    bool separateChannels = true;
};

struct TimelineOptionsInput {
    bool importTempo = false;
    bool importTimeSignature = false;
};

// Aggregated configuration of the MIDI import page.
struct MidiUserInput {
    TextEncodingInput encoding;
    TrackSelectionInput tracks;
    ChannelSeparationInput channels;
    TimelineOptionsInput timeline;
};

#endif // DS_EDITOR_LITE_USERINPUT_H
