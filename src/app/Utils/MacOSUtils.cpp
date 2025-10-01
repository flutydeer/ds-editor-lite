#include "MacOSUtils.h"

#ifdef __APPLE__
#  include <CoreFoundation/CoreFoundation.h>

namespace MacOSUtils {
    template<typename T>
    class UniqueCFRef {
    public:
        explicit UniqueCFRef(T ref) : ref_(ref) {}
        ~UniqueCFRef() {
            if (ref_) {
                CFRelease(ref_);
            }
        }

        T get() const {
            return ref_;
        }

        T release() {
            T temp = ref_;
            ref_ = nullptr;
            return temp;
        }

        explicit operator bool() const {
            return ref_ != nullptr;
        }

        UniqueCFRef(const UniqueCFRef&) = delete;
        UniqueCFRef& operator=(const UniqueCFRef&) = delete;

    private:
        T ref_;
    };

    std::filesystem::path getMainBundlePath() {
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        if (!mainBundle) {
            return {};
        }

        auto bundleURL = UniqueCFRef(CFBundleCopyBundleURL(mainBundle));
        if (!bundleURL) {
            return {};
        }

        char path[PATH_MAX];
        if (!CFURLGetFileSystemRepresentation(bundleURL.get(), true,
                                              reinterpret_cast<UInt8 *>(path), PATH_MAX)) {
            return {};
        }

        return path;
    }
}
#endif
