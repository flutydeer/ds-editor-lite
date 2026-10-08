#ifndef PRONUNCIATIONSTAGE_H
#define PRONUNCIATIONSTAGE_H

namespace lite::synthrt {

    /// Stage that produced a pronunciation.
    ///
    /// The values mirror the hit stages that the language module reports for a converted word, so
    /// that a caller can tell a dictionary hit from a rule or a fallback without depending on the
    /// language module's API. The stage is diagnostic: a caller may ignore it, and a module that
    /// cannot tell how it produced a reading reports Unspecified.
    enum class PronunciationStage {
        /// The language module reported no stage for this word.
        Unspecified,
        /// A dictionary entry.
        Dict,
        /// A model.
        Model,
        /// A rule.
        Rule,
        /// The last resort, which may not match how the word is pronounced.
        Fallback,
        /// A pronunciation that was pinned by the caller and passed through.
        Locked,
    };

}

#endif // PRONUNCIATIONSTAGE_H
