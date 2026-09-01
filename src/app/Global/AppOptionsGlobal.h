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
        Automation,
        DeveloperOptions,

        // Hidden options
        G2pLanguage,
        FillLyric,
        Window,
        // PreviewFunctions,
    };
};

#endif // APPOPTIONSGLOBAL_H
