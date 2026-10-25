#ifndef PRONUNCIATIONSTAGE_H
#define PRONUNCIATIONSTAGE_H

namespace lite::synthrt {

    /// Diagnostic source of a pronunciation.
    enum class PronunciationStage {
        Unspecified,
        Dict,
        Model,
        Rule,
        Fallback,
        Locked,
    };

}

#endif // PRONUNCIATIONSTAGE_H
