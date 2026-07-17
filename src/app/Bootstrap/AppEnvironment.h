#ifndef APPENVIRONMENT_H
#define APPENVIRONMENT_H

// Application-wide Qt environment setup split around QApplication construction.
namespace AppEnvironment {

    // Must be called BEFORE the QApplication object is constructed:
    // message handler, env vars, HighDPI policy, platform attributes.
    void preInit();

    // Must be called AFTER the QApplication object is constructed:
    // app metadata, UI effects, style, color scheme, widget defaults, fonts.
    void postInit();

} // namespace AppEnvironment

#endif // APPENVIRONMENT_H
