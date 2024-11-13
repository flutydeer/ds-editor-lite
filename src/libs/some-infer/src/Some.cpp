#include <some-infer/Some.h>

#include <CDSPResampler.h>
#include <sndfile.hh>

#include <iostream>

#include <audio-util/Resample.h>
#include <audio-util/Slicer.h>
#include <some-infer/SomeModel.h>

namespace Some
{
    Some::Some(const std::filesystem::path &modelPath, ExecutionProvider provider, int device_id) {
        m_some = std::make_unique<SomeModel>(modelPath, provider, device_id);

        if (!m_some) {
            std::cout << "Cannot load ASR Model, there must be files model.onnx and vocab.txt" << std::endl;
        }
    }

    Some::~Some() = default;

    bool Some::get_midi(AudioUtil::SF_VIO sf_vio, std::vector<float> &note_midi, std::vector<bool> &note_rest,
                        std::vector<float> &note_dur, std::string &msg, void (*progressChanged)(int)) const {
        if (!m_some) {
            return false;
        }

        SndfileHandle sf(sf_vio.vio, &sf_vio.data, SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
        AudioUtil::Slicer slicer(&sf, -40, 5000, 300, 10, 1000);

        const auto chunks = slicer.slice();

        if (chunks.empty()) {
            msg = "slicer: no audio chunks for output!";
            return false;
        }

        const auto frames = sf.frames();
        const auto totalSize = frames;

        int processedFrames = 0; // To track processed frames

        for (const auto &chunk : chunks) {
            const auto beginFrame = chunk.first;
            const auto endFrame = chunk.second;
            const auto frameCount = endFrame - beginFrame;
            if (frameCount <= 0 || beginFrame > totalSize || endFrame > totalSize) {
                continue;
            }

            AudioUtil::SF_VIO sfChunk;
            auto wf = SndfileHandle(sfChunk.vio, &sfChunk.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
            sf.seek(static_cast<sf_count_t>(beginFrame), SEEK_SET);
            std::vector<float> tmp(frameCount);
            sf.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            const auto bytesWritten = wf.write(tmp.data(), static_cast<sf_count_t>(tmp.size()));

            std::vector<float> temp_midi;
            std::vector<bool> temp_rest;
            std::vector<float> temp_dur;

            const bool success = m_some->forward(tmp, temp_midi, temp_rest, temp_dur, msg);
            if (!success)
                return false;
            note_midi.insert(note_midi.end(), temp_midi.begin(), temp_midi.end());
            note_rest.insert(note_rest.end(), temp_rest.begin(), temp_rest.end());
            note_dur.insert(note_dur.end(), temp_dur.begin(), temp_dur.end());

            // Update the processed frames and calculate progress
            processedFrames += static_cast<int>(frameCount);
            int progress = static_cast<int>((static_cast<float>(processedFrames) / totalSize) * 100);

            // Call the progress callback with the updated progress
            if (progressChanged) {
                progressChanged(progress); // Trigger the callback with the progress value
            }
        }
        return true;
    }

    bool Some::get_midi(const std::filesystem::path &filepath, std::vector<float> &note_midi,
                        std::vector<bool> &note_rest, std::vector<float> &note_dur, std::string &msg,
                        void (*progressChanged)(int)) const {
        return get_midi(AudioUtil::resample(filepath, 1, 44100), note_midi, note_rest, note_dur, msg, progressChanged);
    }
} // namespace Some
