#include "FlacDecoder.h"

#include <iostream>

namespace AudioUtil
{
    FLACDecoder::FLACDecoder(const std::filesystem::path &filepath) : filepath(filepath) {
        input_stream.open(filepath, std::ios::binary);
        if (!input_stream.is_open()) {
            std::cerr << "Unable to open FLAC file: " << filepath << std::endl;
        }
    }

    FLACDecoder::~FLACDecoder() {
        if (input_stream.is_open()) {
            input_stream.close();
        }
    }

    bool FLACDecoder::init_decoder() {
        const FLAC__StreamDecoderInitStatus initStatus = this->init();
        if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            std::cerr << "FLAC decoder initialization failed: " << initStatus << std::endl;
            return false;
        }
        return true;
    }

    void FLACDecoder::metadata_callback(const FLAC__StreamMetadata *metadata) {
        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
            channels = metadata->data.stream_info.channels;
            sampleRate = metadata->data.stream_info.sample_rate;
            totalSamples = metadata->data.stream_info.total_samples;
            std::cout << "FLAC channels: " << channels << ", sampleRate: " << sampleRate
                      << ", totalSamples: " << totalSamples << std::endl;
        }
    }

    FLAC__StreamDecoderReadStatus FLACDecoder::read_callback(FLAC__byte buffer[], size_t *bytes) {
        input_stream.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(*bytes));
        *bytes = input_stream.gcount();

        if (input_stream.eof())
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

        return *bytes > 0 ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    FLAC__StreamDecoderWriteStatus FLACDecoder::write_callback(const FLAC__Frame *frame,
                                                               const FLAC__int32 *const buffer[]) {
        const unsigned samples = frame->header.blocksize;
        const unsigned channels = frame->header.channels;

        std::vector<short> buffer_out(samples);

        for (unsigned i = 0; i < samples; ++i) {
            int32_t sum = 0;
            for (unsigned ch = 0; ch < channels; ++ch) {
                sum += buffer[ch][i];
            }
            buffer_out[i] = static_cast<short>(sum / channels);
        }

        if (sndfile.write(buffer_out.data(), samples) != samples) {
            std::cerr << "An error occurred while writing to the WAV file." << std::endl;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    void FLACDecoder::error_callback(FLAC__StreamDecoderErrorStatus status) {
        std::cerr << "Error decoding FLAC file: " << status << std::endl;
        switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            std::cerr << "Sync lost!" << std::endl;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            std::cerr << "Invalid file header!" << std::endl;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            std::cerr << "Frame CRC mismatch!" << std::endl;
            break;
        default:
            std::cerr << "Unknown error!" << std::endl;
            break;
        }
    }

    void FLACDecoder::set_sndfile(const SndfileHandle &sndfile) { this->sndfile = sndfile; }

    unsigned FLACDecoder::get_channels() const { return channels; }

    unsigned FLACDecoder::get_sample_rate() const { return sampleRate; }

    unsigned long long FLACDecoder::get_total_samples() const { return totalSamples; }

    void write_flac_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio) {
        FLACDecoder decoder(filepath.string());

        if (!decoder.init_decoder()) {
            std::cerr << "FLAC decoder initialization failed." << std::endl;
            return;
        }

        if (!decoder.process_until_end_of_metadata()) {
            std::cerr << "Metadata read failed." << std::endl;
            return;
        }

        const SndfileHandle sndfile(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1,
                                    static_cast<int>(decoder.get_sample_rate()));
        if (!sndfile) {
            std::cerr << "Unable to open output file for writing. error message: " << sndfile.strError() << std::endl;
            return;
        }
        decoder.set_sndfile(sndfile);

        bool processStatus = decoder.process_until_end_of_stream();
        if (!processStatus) {
            std::cerr << "FLAC decode fail." << std::endl;
        } else {
            std::cout << "FLAC decode success." << std::endl;
        }
    }

} // namespace AudioUtil
