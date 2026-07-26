#ifndef LITE_CORE_REGISTRY_H
#define LITE_CORE_REGISTRY_H

// A tiny, type-keyed service registry. It decouples singleton *lookup* from any
// concrete composition root: an owner (e.g. the application's AppContext)
// registers its instances here, and LITE_SINGLETON's instance() looks them up.
//
// Keys are per-type static addresses, so no RTTI is required and it works for
// any type without pre-registration of a type list. Everything is linked into a
// single binary here, so each type's key is a single stable address.
class Registry {
public:
    template <typename T>
    static T *instance() {
        return static_cast<T *>(get(key<T>()));
    }

    template <typename T>
    static void add(T *object) {
        set(key<T>(), object);
    }

    template <typename T>
    static void remove() {
        set(key<T>(), nullptr);
    }

    static void clear();

private:
    template <typename T>
    static const void *key() {
        static const char anchor = 0;
        return &anchor;
    }

    static void *get(const void *key);
    static void set(const void *key, void *value);
};

#endif // LITE_CORE_REGISTRY_H
