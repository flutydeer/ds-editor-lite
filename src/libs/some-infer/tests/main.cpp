#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>
#include <synthrt/Core/Contribute.h>
#include <synthrt/Core/NamedObject.h>
#include <synthrt/Core/SynthUnit.h>
#include <dsinfer/Inference/InferenceDriverPlugin.h>
#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>

#include <some-infer/Some.h>

#include "wolf-midi/MidiFile.h"

using EP = ds::Api::Onnx::ExecutionProvider;

static void progressChanged(const int progress) { std::cout << "progress: " << progress << "%" << std::endl; }

static srt::Expected<void> initializeSU(srt::SynthUnit &su, EP ep, int deviceIndex) {
    // Get basic directories
    auto appDir = stdc::system::application_directory();
    auto defaultPluginDir =
        appDir.parent_path() / _TSTR("lib") / _TSTR("plugins") / _TSTR("dsinfer");

    // Set default plugin directories
    su.addPluginPath("org.openvpi.InferenceDriver", defaultPluginDir / _TSTR("inferencedrivers"));

    // Load driver
    auto plugin = su.plugin<ds::InferenceDriverPlugin>("onnx");
    if (!plugin) {
        return srt::Error(srt::Error::FileNotOpen,"failed to load inference driver");
    }

    auto onnxDriver = plugin->create();
    auto onnxArgs = srt::NO<ds::Api::Onnx::DriverInitArgs>::create();

    onnxArgs->ep = ep;
    onnxArgs->runtimePath = plugin->path().parent_path() / _TSTR("runtimes");
    onnxArgs->deviceIndex = deviceIndex;

    if (auto exp = onnxDriver->initialize(onnxArgs); !exp) {
        return srt::Error(srt::Error::FileNotOpen,
            stdc::formatN(R"(failed to initialize onnx driver: %1)", exp.error().message()));
    }

    // Add driver
    auto &ic = *su.category("inference");
    ic.addObject("dsdriver", onnxDriver);
    return srt::Expected<void>();
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <model_path> <wav_path> <dml/cpu> <device_id> <out_midi_path> <tempo:float>" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    const std::filesystem::path wavPath = argv[2];
    const std::string provider = argv[3];
    const int device_id = std::stoi(argv[4]);
    const std::filesystem::path outMidiPath = argv[5];
    const float tempo = std::stof(argv[6]);

    const auto someProvider = [](const std::string &provider_) -> EP {
        auto provider_lower = stdc::to_lower(provider_);
        if (provider_lower == "dml" || provider_lower == "directml") {
            return EP::DMLExecutionProvider;
        }
        if (provider_lower == "cuda") {
            return EP::CUDAExecutionProvider;
        }
        if (provider_lower == "coreml") {
            return EP::CoreMLExecutionProvider;
        }
        return EP::CPUExecutionProvider;
    }(provider);

    srt::SynthUnit su;
    if (auto exp = initializeSU(su, someProvider, device_id); !exp) {
        std::cerr << "failed to initialize SynthUnit: " << exp.error().message() << std::endl;
        return 1;
    }
    Some::Some some(&su);
    if (auto exp = some.open(modelPath); !exp) {
        std::cerr << "failed to open SOME model " << modelPath << ": " << exp.error().message() << std::endl;
        return 1;
    }

    std::vector<Some::Midi> midis;
    std::string msg;

    const bool success = some.get_midi(wavPath, midis, tempo, msg, progressChanged);

    if (success) {
        Midi::MidiFile midi;
        midi.setFileFormat(1);
        midi.setDivisionType(Midi::MidiFile::DivisionType::PPQ);
        midi.setResolution(480);

        midi.createTrack();

        midi.createTimeSignatureEvent(0, 0, 4, 4);
        midi.createTempoEvent(0, 0, tempo);

        std::vector<char> trackName;
        std::string str = "Some";
        trackName.insert(trackName.end(), str.begin(), str.end());

        midi.createTrack();
        midi.createMetaEvent(1, 0, Midi::MidiEvent::MetaNumbers::TrackName, trackName);

        for (const auto &[note, start, duration] : midis) {
            midi.createNoteOnEvent(1, start, 0, note, 64);
            midi.createNoteOffEvent(1, start + duration, 0, note, 64);
        }

        midi.save(outMidiPath);
    } else {
        std::cerr << "Error: " << msg << std::endl;
        return 1;
    }

    return 0;
}
