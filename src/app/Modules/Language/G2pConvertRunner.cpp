#include "G2pConvertRunner.h"

std::vector<srt::g2p::G2pRes>
    G2pConvertRunner::convert(const srt::g2p::LanguageService &service,
                              const std::vector<srt::g2p::G2pInput> &requests) {
    return service.convertLyric(requests);
}
