#ifndef FLACDECODER_H
#define FLACDECODER_H

#include <FLAC++/decoder.h>
#include <audio-util/Util.h>
#include <fcntl.h>
#include <fstream>
#include <mpg123.h>

namespace AudioUtil
{
    class FLACDecoder final : public FLAC::Decoder::Stream {
    public:
        explicit FLACDecoder(const std::filesystem::path &filepath);

        bool init_decoder();
        void metadata_callback(const FLAC__StreamMetadata *metadata) override;
        FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t *bytes) override;
        FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *frame,
                                                      const FLAC__int32 *const buffer[]) override;
        void error_callback(FLAC__StreamDecoderErrorStatus status) override;

        void set_sndfile(const SndfileHandle &sndfile);
        unsigned get_channels() const override;
        unsigned get_sample_rate() const override;
        unsigned get_bits_per_sample() const override;
        unsigned long long get_total_samples() const override;

        ~FLACDecoder() override;

    private:
        std::ifstream input_stream;
        SndfileHandle sndfile;
        std::filesystem::path filepath;
        unsigned channels = 0;
        unsigned sampleRate = 0;
        unsigned frames = 0;
        unsigned bitsPerSample = 0;
        std::vector<int16_t> buffer_out_16;
        std::vector<short> buffer_out_24;
        std::vector<int32_t> buffer_out_32;
    };

    void write_flac_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio);

} // namespace AudioUtil

#endif // FLACDECODER_H
