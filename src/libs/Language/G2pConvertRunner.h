#ifndef G2PCONVERTRUNNER_H
#define G2PCONVERTRUNNER_H

#include <vector>

#include <synthrt/G2P/LanguageService.h>

namespace G2pConvertRunner {
    /// Run native G2P requests as a batch. The caller validates the result count and handles
    /// fallback.
    std::vector<srt::g2p::G2pRes> convert(const srt::g2p::LanguageService &service,
                                          const std::vector<srt::g2p::G2pInput> &requests);
}

#endif // G2PCONVERTRUNNER_H
