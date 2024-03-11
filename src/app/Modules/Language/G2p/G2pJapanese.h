#ifndef DS_EDITOR_LITE_G2PJAPANESE_H
#define DS_EDITOR_LITE_G2PJAPANESE_H

#include "jpg2p.h"
#include "Utils/Singleton.h"

class G2pJapanese final : public Singleton<G2pJapanese>, public IKg2p::JpG2p {};

#endif // DS_EDITOR_LITE_G2PJAPANESE_H
