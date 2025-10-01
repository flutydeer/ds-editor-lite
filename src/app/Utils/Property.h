//
// Created by fluty on 24-8-13.
//

#ifndef PROPERTY_H
#define PROPERTY_H

#define qSignalCallback(SignalName) [this](const auto &value) { emit SignalName(value); }

#include <functional>

template <typename T>
class Property {

public:
    virtual ~Property() = default;
    [[nodiscard]] virtual T get() const;
    [[nodiscard]] operator T() const; // NOLINT(*-explicit-constructor)
    Property &operator=(const Property &other);
    Property(const T &value) : value(value){}; // NOLINT(*-explicit-constructor)

    Property() {
        value = T();
    }

    // public slots:
    virtual void set(const T &newValue);
    void onChanged(std::function<void(const T &newValue)> callback);

    // signals:
    //     void valueChanged(const T &newValue);

protected:
    T value;

private:
    std::function<void(const T &newValue)> notify;
};

template <typename T>
T Property<T>::get() const {
    return value;
}

template <typename T>
[[nodiscard]] Property<T>::operator T() const {
    return get();
}

template <typename T>
Property<T> &Property<T>::operator=(const Property &other) {
    set(other.value);
    return *this;
}

template <typename T>
void Property<T>::set(const T &newValue) {
    if (value != newValue) {
        value = newValue;
        if (notify)
            notify(newValue);
    }
}

template <typename T>
void Property<T>::onChanged(std::function<void(const T &newValue)> callback) {
    notify = callback;
}

#endif // PROPERTY_H
