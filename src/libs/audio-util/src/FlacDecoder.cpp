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
            frames = metadata->data.stream_info.total_samples;
            bitsPerSample = metadata->data.stream_info.bits_per_sample;
            std::cout << "FLAC channels: " << channels << ", sampleRate: " << sampleRate << ", frames: " << frames
                      << ", bitsPerSample: " << bitsPerSample << std::endl;
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


        if (frame->header.bits_per_sample <= 16) {
            if (buffer_out_16.size() < channels * samples) {
                buffer_out_16.resize(channels * samples, 0);
            }
            for (unsigned int chn = 0; chn < channels; ++chn) {
                for (unsigned int s = 0; s < frame->header.blocksize; s++) {
                    buffer_out_16[channels * s + chn] =
                        static_cast<short>(buffer[chn][s] >> (frame->header.bits_per_sample == 8 ? 8 : 0));
                }
            }

            if (sndfile.write(buffer_out_16.data(), channels * samples) != channels * samples) {
                std::cerr << "An error occurred while writing to the WAV file." << std::endl;
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
        } else if (frame->header.bits_per_sample == 24) {
            if (buffer_out_24.size() < channels * samples) {
                buffer_out_24.resize(channels * samples, 0);
            }
            for (unsigned int chn = 0; chn < channels; ++chn) {
                for (unsigned int s = 0; s < frame->header.blocksize; s++) {
                    buffer_out_24[channels * s + chn] = static_cast<short>(buffer[chn][s] >> 8);
                }
            }

            if (sndfile.write(buffer_out_24.data(), channels * samples) != channels * samples) {
                std::cerr << "An error occurred while writing to the WAV file." << std::endl;
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
        } else if (frame->header.bits_per_sample == 32) {
            if (buffer_out_32.size() < channels * samples) {
                buffer_out_32.resize(channels * samples, 0);
            }
            for (unsigned int chn = 0; chn < channels; ++chn) {
                for (unsigned int s = 0; s < frame->header.blocksize; s++) {
                    buffer_out_32[channels * s + chn] = buffer[chn][s];
                }
            }

            if (sndfile.write(buffer_out_32.data(), channels * samples) != channels * samples) {
                std::cerr << "An error occurred while writing to the WAV file." << std::endl;
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
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

    unsigned FLACDecoder::get_bits_per_sample() const { return bitsPerSample; }

    unsigned long long FLACDecoder::get_total_samples() const { return frames; }

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

        if (decoder.get_bits_per_sample() == 32) {
            sf_vio.info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
        } else if (decoder.get_bits_per_sample() == 24) {
            sf_vio.info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
        } else if (decoder.get_bits_per_sample() == 8) {
            sf_vio.info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_S8;
        } else {
            sf_vio.info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        }

        sf_vio.info.samplerate = static_cast<int>(decoder.get_sample_rate());
        sf_vio.info.channels = static_cast<int>(decoder.get_channels());
        sf_vio.info.frames = static_cast<sf_count_t>(decoder.get_total_samples());
        sf_vio.info.sections = 1;
        sf_vio.info.seekable = 1;

        const SndfileHandle sndfile(sf_vio.vio, &sf_vio.data, SFM_WRITE, sf_vio.info.format, sf_vio.info.channels,
                                    sf_vio.info.samplerate);
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
