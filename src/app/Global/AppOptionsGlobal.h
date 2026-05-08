//
// Created by FlutyDeer on 2025/2/26.
//

#ifndef APPOPTIONSGLOBAL_H
#define APPOPTIONSGLOBAL_H

namespace AppOptionsGlobal {
    enum Option {
        All,
        General,
        Audio,
        Midi,
        Appearance,
        Inference,
        DeveloperOptions,

        // Hidden options
        Language,
        FillLyric,
        // PreviewFunctions,
    };
};

#endif // APPOPTIONSGLOBAL_H