#ifndef G2PINPUTADAPTER_H
#define G2PINPUTADAPTER_H

#include <string>

#include <synthrt/G2P/Base/LangCommon.h>
#include <synthrt/G2P/LanguageRoute.h>

namespace G2pInputAdapter {
    srt::g2p::G2pInput fromRoute(std::string lyric, const srt::g2p::LanguageRoute &route);
}

#endif // G2PINPUTADAPTER_H
