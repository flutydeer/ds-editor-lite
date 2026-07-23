#ifndef MACOSUTILS_H
#define MACOSUTILS_H

#ifdef __APPLE__
#  include <filesystem>

namespace MacOSUtils {
    std::filesystem::path getMainBundlePath();
}
#endif

#endif // MACOSUTILS_H
