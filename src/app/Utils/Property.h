//
// Created by fluty on 24-8-13.
//

#ifndef PROPERTY_H
#define PROPERTY_H

template <typename T>
class Property {

public:
    virtual ~Property() = default;
    virtual [[nodiscard]] T get() const;
    [[nodiscard]] operator T() const;
    Property &operator=(const Property &other);
    Property(const T &value) : value(value){};

    // public slots:
    virtual void set(const T &newValue);
    void setNotifyCallback(std::function<void(const T &newValue)> func);

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
        // emit valueChanged(newValue);
    }
}
template <typename T>
void Property<T>::setNotifyCallback(std::function<void(const T &newValue)> func) {
    notify = func;
}

#endif // PROPERTY_H
