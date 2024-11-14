#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <some-infer/Some.h>

#include "MidiFile.h"

static void progressChanged(const int progress) { std::cout << "progress: " << progress << "%" << std::endl; }

int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <wav_path> <out_midi_path> <tempo:float>" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    const std::filesystem::path wavPath = argv[2];
    const std::filesystem::path outMidiPath = argv[3];
    const float tempo = std::stof(argv[4]);

    const Some::Some some(modelPath, Some::ExecutionProvider::DML, 1);

    std::vector<Some::Midi> midis;
    std::string msg;

    bool success = some.get_midi(wavPath, midis, tempo, msg, progressChanged);

    if (success) {
        MidiFile midi;
        midi.setFileFormat(1);
        midi.setDivisionType(MidiFile::DivisionType::PPQ);
        midi.setResolution(480);

        midi.createTrack();

        // timeSignature
        std::vector<char> buf(4);
        buf[0] = static_cast<char>(4);
        buf[1] = static_cast<char>(4);
        buf[2] = 24;
        buf[3] = 8;
        midi.createMetaEvent(0, 0, MidiEvent::TimeSignature, buf);
        midi.createTempoEvent(0, 0, tempo);

        std::vector<char> trackName;
        std::string str = "Some";
        trackName.insert(trackName.end(), str.begin(), str.end());

        midi.createTrack();
        midi.createMetaEvent(1, 0, MidiEvent::MetaNumbers::TrackName, trackName);

        for (const auto &[note, start, duration] : midis) {
            midi.createNoteOnEvent(1, start, 0, note, 64);
            midi.createNoteOffEvent(1, start + duration, 0, note, 64);
        }

        midi.save(outMidiPath);
    } else {
        std::cerr << "Error: " << msg << std::endl;
    }

    return 0;
}
