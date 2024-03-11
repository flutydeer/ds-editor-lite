#ifndef DS_EDITOR_LITE_G2PMANDARIN_H
#define DS_EDITOR_LITE_G2PMANDARIN_H

#include "mandarin.h"
#include "Utils/Singleton.h"

class G2pMandarin final : public Singleton<G2pMandarin>, public IKg2p::Mandarin {};

#endif // DS_EDITOR_LITE_G2PMANDARIN_H
