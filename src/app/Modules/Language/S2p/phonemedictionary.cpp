#include "phonemedictionary.h"

#include <fstream>
#include <algorithm>

namespace dsutils {

    bool PhonemeDictionary::load(const std::filesystem::path &path, std::string *error) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            if (error)
                *error = "failed to open file";
            return false;
        }

        file.seekg(0, std::ios::end);
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        m_filebuf.resize(file_size + 1); // +1 for null terminator
        if (!file.read(m_filebuf.data(), file_size)) {
            if (error)
                *error = "failed to read file";
            return false;
        }
        m_filebuf[file_size] = '\n';
        m_map.clear();

        // Parse the buffer
        const auto buffer_begin = m_filebuf.data();
        const auto buffer_end = buffer_begin + m_filebuf.size();

        // Estimate line numbers if the file is too large
        static constexpr const size_t larget_file_size = 1 * 1024 * 1024;
        if (file_size > larget_file_size) {
            size_t line_cnt = std::count(buffer_begin, buffer_end, '\n') + 1;
            m_map.reserve(line_cnt);
        }

        // Traverse lines
        {
            auto start = buffer_begin;
            while (start < buffer_end) {
                while (start < buffer_end && (*start == '\r' || *start == '\n')) {
                    *start = '\0';
                    start++;
                }

                char *value_start = nullptr;
                int value_cnt = 0;

                // Find next line break
                                auto p = start + 1;
                while (p < buffer_end) {
                    switch (*p) {
                        case '\t': {
                            if (!value_start) {
                                value_start = p + 1;
                            }
                            value_cnt++;
                            *p = '\0';
                            break;
                        }
                        case ' ': {
                            if (!value_start) {
                                break;
                            }
                            value_cnt++;
                            *p = '\0';
                            break;
                        }
                        case '\r':
                        case '\n': {
                            *p = '\0';
                            goto out_line_loop;
                        }
                        default:
                            break;
                    }
                    ++p;
                }
            out_line_loop:

                // Value not found, maybe EOF
                if (!value_start) {
                    continue;
                }

                std::string_view key(start, value_start - 1 - start);
                m_map[key] = Entry{int(value_start - buffer_begin), value_cnt};
                start = p + 1;
            }
        }
        return true;
    }

    void PhonemeDictionary::readEntry(Entry entry, std::string_view out[], int cnt) const {
        auto p = m_filebuf.data() + entry.offset;
        auto q = p;
        int i = 0;
        int min = std::min(cnt, entry.count);
        while (i < min) {
            if (*q == '\0') {
                out[i++] = p;
                p = q + 1;
            }
            q++;
        }
    }

}