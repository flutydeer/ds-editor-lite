#ifndef DS_EDITOR_LITE_G2PCANTONESE_H
#define DS_EDITOR_LITE_G2PCANTONESE_H

#include "cantonese.h"
#include "Utils/Singleton.h"

class G2pCantonese final : public Singleton<G2pCantonese>, public IKg2p::Cantonese {};

#endif // DS_EDITOR_LITE_G2PCANTONESE_H
