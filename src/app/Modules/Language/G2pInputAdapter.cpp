#include "G2pInputAdapter.h"

namespace G2pInputAdapter {
    srt::g2p::G2pInput fromRoute(std::string lyric, const srt::g2p::LanguageRoute &route) {
        return {std::move(lyric), route.g2pId, route.g2pContext, route.g2pContextVersion};
    }
}
