#include "Mp3Decoder.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mpg123.h>
#include <vector>

#include <CDSPResampler.h>

namespace AudioUtil
{
    void write_mp3_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio) {
        if (mpg123_init() != MPG123_OK) {
            std::cerr << "Failed to initialize mpg123" << std::endl;
            return;
        }

        mpg123_handle *mh = mpg123_new(nullptr, nullptr);
        if (mh == nullptr) {
            std::cerr << "Failed to create mpg123 handle" << std::endl;
            mpg123_exit();
            return;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open MP3 file: " << filepath << std::endl;
            mpg123_delete(mh);
            mpg123_exit();
            return;
        }

        std::vector<unsigned char> mp3_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        if (mpg123_open_feed(mh) != MPG123_OK) {
            std::cerr << "Failed to open feed" << std::endl;
            mpg123_delete(mh);
            mpg123_exit();
            return;
        }

        size_t pos = 0;
        while (pos < mp3_data.size()) {
            constexpr size_t buffer_size = 8192;
            unsigned char buffer[buffer_size];
            const size_t feed_size = min(buffer_size, mp3_data.size() - pos);
            std::copy_n(mp3_data.begin() + static_cast<long long>(pos), feed_size, buffer);

            const int feed_result = mpg123_feed(mh, buffer, feed_size);
            if (feed_result != MPG123_OK) {
                std::cerr << "Failed to feed data to mpg123: " << mpg123_strerror(mh) << std::endl;
                break;
            }
            pos += feed_size;
        }

        long rate;
        int channels, encoding;
        if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
            std::cerr << "Failed to get format from mpg123: " << mpg123_strerror(mh) << std::endl;
            mpg123_delete(mh);
            mpg123_exit();
            return;
        }
        std::cout << "Rate: " << rate << ", Channels: " << channels << ", Encoding: " << encoding << std::endl;

        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, channels, rate);

        std::vector<short> pcm_buffer(8192);
        size_t done;
        while (true) {
            int err = mpg123_read(mh, pcm_buffer.data(), pcm_buffer.size() * sizeof(short), &done);
            if (err != MPG123_OK) {
                if (done == 0) {
                    break;
                }
                // std::cerr << "Error while decoding MP3: " << mpg123_strerror(mh) << std::endl;
                break;
            }

            outBuf.write(pcm_buffer.data(), static_cast<sf_count_t>(static_cast<sf_count_t>(done) / sizeof(short)));
        }

        mpg123_delete(mh);
        mpg123_exit();
        std::cout << "Mp3 decode success." << std::endl;
    }
} // namespace AudioUtil
