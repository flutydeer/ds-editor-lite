//
// Created by FlutyDeer on 2025/6/1.
//

#ifndef EXPECTED_H
#define EXPECTED_H

#include <variant>
#include <stdexcept>
#include <utility>
#include <type_traits>

template <typename T, typename E>
class Expected {
    static_assert(!std::is_same_v<T, E>, "T and E must be different types");
    static_assert(!std::is_void_v<T>, "T cannot be void");
    static_assert(!std::is_reference_v<T>, "T cannot be a reference type");
    static_assert(!std::is_reference_v<E>, "E cannot be a reference type");

    std::variant<T, E> data;
    bool hasValue;

public:
    // Constructors
    Expected() : data(), hasValue(false) {
        if constexpr (std::is_default_constructible_v<T>) {
            data.template emplace<T>();
            hasValue = true;
        }
    }

    Expected(const T &value) : data(value), hasValue(true) {
    }

    Expected(T &&value) : data(std::move(value)), hasValue(true) {
    }

    Expected(const E &error) : data(error), hasValue(false) {
    }

    Expected(E &&error) : data(std::move(error)), hasValue(false) {
    }

    // Copy/move constructors
    Expected(const Expected &) = default;
    Expected(Expected &&) = default;

    // Assignment operators
    Expected &operator=(const Expected &) = default;
    Expected &operator=(Expected &&) = default;

    // Java-style methods
    bool isPresent() const noexcept {
        return hasValue;
    }

    explicit operator bool() const noexcept {
        return isPresent();
    }

    T &get() & {
        if (!hasValue) {
            throw std::runtime_error("Expected doesn't contain a value");
        }
        return std::get<T>(data);
    }

    const T &get() const & {
        if (!hasValue) {
            throw std::runtime_error("Expected doesn't contain a value");
        }
        return std::get<T>(data);
    }

    T &&get() && {
        if (!hasValue) {
            throw std::runtime_error("Expected doesn't contain a value");
        }
        return std::get<T>(std::move(data));
    }

    const E &getError() const & {
        if (hasValue) {
            throw std::runtime_error("Expected doesn't contain an error");
        }
        return std::get<E>(data);
    }

    E &&getError() && {
        if (hasValue) {
            throw std::runtime_error("Expected doesn't contain an error");
        }
        return std::get<E>(std::move(data));
    }

    T orElse(const T &defaultValue) const & {
        return hasValue ? std::get<T>(data) : defaultValue;
    }

    T orElse(T &&defaultValue) const & {
        return hasValue ? std::get<T>(data) : std::move(defaultValue);
    }

    T orElse(T &&defaultValue) && {
        return hasValue ? std::get<T>(std::move(data)) : std::move(defaultValue);
    }

    template <typename F>
    T orElseGet(F &&supplier) const & {
        static_assert(std::is_invocable_r_v<T, F>, "Supplier must return type T");
        return hasValue ? std::get<T>(data) : std::forward<F>(supplier)();
    }

    template <typename F>
    T orElseGet(F &&supplier) && {
        static_assert(std::is_invocable_r_v<T, F>, "Supplier must return type T");
        return hasValue ? std::get<T>(std::move(data)) : std::forward<F>(supplier)();
    }

    template <typename F>
    Expected<T, E> otherwise(F &&supplier) const & {
        static_assert(std::is_invocable_r_v<Expected<T, E>, F>,
                      "Supplier must return Expected<T, E>");
        return hasValue ? *this : std::forward<F>(supplier)();
    }

    template <typename F>
    Expected<T, E> otherwise(F &&supplier) && {
        static_assert(std::is_invocable_r_v<Expected<T, E>, F>,
                      "Supplier must return Expected<T, E>");
        return hasValue ? std::move(*this) : std::forward<F>(supplier)();
    }

    template <typename F>
    auto map(F &&mapper) const & {
        using U = std::invoke_result_t<F, const T &>;
        if (!hasValue) {
            return Expected<U, E>(std::get<E>(data));
        }
        return Expected<U, E>(std::forward<F>(mapper)(std::get<T>(data)));
    }

    template <typename F>
    auto map(F &&mapper) && {
        using U = std::invoke_result_t<F, T &&>;
        if (!hasValue) {
            return Expected<U, E>(std::get<E>(std::move(data)));
        }
        return Expected<U, E>(std::forward<F>(mapper)(std::get<T>(std::move(data))));
    }

    template <typename F>
    auto flatMap(F &&mapper) const & {
        using ResultType = std::invoke_result_t<F, const T &>;
        static_assert(is_expected_v<ResultType>, "Mapper must return Expected");
        if (!hasValue) {
            return ResultType(std::get<E>(data));
        }
        return std::forward<F>(mapper)(std::get<T>(data));
    }

    template <typename F>
    auto flatMap(F &&mapper) && {
        using ResultType = std::invoke_result_t<F, T &&>;
        static_assert(is_expected_v<ResultType>, "Mapper must return Expected");
        if (!hasValue) {
            return ResultType(std::get<E>(std::move(data)));
        }
        return std::forward<F>(mapper)(std::get<T>(std::move(data)));
    }

    template <typename F>
    Expected<T, E> filter(F &&predicate) const & {
        static_assert(std::is_invocable_r_v<bool, F, const T &>, "Predicate must return bool");
        if (!hasValue) {
            return *this;
        }
        if (std::forward<F>(predicate)(std::get<T>(data))) {
            return *this;
        }
        return Expected<T, E>();
    }

    template <typename F>
    Expected<T, E> filter(F &&predicate) && {
        static_assert(std::is_invocable_r_v<bool, F, T &&>, "Predicate must return bool");
        if (!hasValue) {
            return std::move(*this);
        }
        if (std::forward<F>(predicate)(std::get<T>(std::move(data)))) {
            return std::move(*this);
        }
        return Expected<T, E>();
    }

    template <typename F>
    Expected<T, E> onSuccess(F &&consumer) const & {
        static_assert(std::is_invocable_v<F, const T &>, "Consumer must accept const T&");
        if (hasValue) {
            std::forward<F>(consumer)(std::get<T>(data));
        }
        return *this;
    }

    template <typename F>
    Expected<T, E> onSuccess(F &&consumer) && {
        static_assert(std::is_invocable_v<F, T &&>, "Consumer must accept T&&");
        if (hasValue) {
            std::forward<F>(consumer)(std::get<T>(std::move(data)));
        }
        return std::move(*this);
    }

    template <typename F>
    Expected<T, E> onFailure(F &&consumer) const & {
        static_assert(std::is_invocable_v<F, const E &>, "Consumer must accept const E&");
        if (!hasValue) {
            std::forward<F>(consumer)(std::get<E>(data));
        }
        return *this;
    }

    template <typename F>
    Expected<T, E> onFailure(F &&consumer) && {
        static_assert(std::is_invocable_v<F, E &&>, "Consumer must accept E&&");
        if (!hasValue) {
            std::forward<F>(consumer)(std::get<E>(std::move(data)));
        }
        return std::move(*this);
    }

private:
    template <typename U>
    static constexpr bool is_expected_v = false;

    template <typename U, typename V>
    static constexpr bool is_expected_v<Expected<U, V>> = true;
};

#endif //EXPECTED_H