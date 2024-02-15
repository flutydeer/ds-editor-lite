#ifndef DS_EDITOR_LITE_G2PJAPANESE_H
#define DS_EDITOR_LITE_G2PJAPANESE_H

#include "jpg2p.h"
#include "Utils/QSingleton.h"

class G2pJapanese : public QSingleton<G2pJapanese>, public IKg2p::JpG2p {};

#endif // DS_EDITOR_LITE_G2PJAPANESE_H
