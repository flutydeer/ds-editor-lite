//
// Created by fluty on 24-8-3.
//

#ifndef LINQ_H
#define LINQ_H

#define L_PRED(ParamName, Predicate) [=](const auto &(ParamName)) { return Predicate; }

#include <algorithm>
#include <iterator>
#include <QList>

namespace Linq {
    // template <typename TContainer, typename TPredicate>
    // static TContainer where(const TContainer &container, TPredicate predicate) {
    //     TContainer result;
    //     std::copy_if(container.begin(), container.end(), std::back_inserter(result), predicate);
    //     return result;
    // }
    template <typename TContainer, typename TPredicate>
    auto where(const TContainer &container, TPredicate predicate)
        -> QList<std::decay_t<decltype(*container.begin())>> {
        // using TItem = std::decay_t<decltype(*container.begin())>;
        // QList<TItem> result;
        QList<std::decay_t<decltype(*container.begin())>> result;
        std::copy_if(container.begin(), container.end(), std::back_inserter(result), predicate);
        return result;
    }

    template <typename TContainer, typename TKeys, typename TKeyGetter>
    auto groupBy(const TContainer &container, const TKeys &keys, TKeyGetter getter) {
        using TItem = std::decay_t<decltype(*container.begin())>;
        using TKey = std::decay_t<decltype(*keys.begin())>;

        QMap<TKey, QList<TItem>> map;
        for (const auto &key : keys)
            map.insert(key, QList<TItem>());

        for (const auto &item : container) {
            for (const auto &key : keys) {
                if (getter(item) == key) {
                    map[key].append(item);
                    break;
                }
            }
        }

        return map;
    }

    template <typename TContainer, typename TSelector>
    auto selectMany(const TContainer &container, TSelector selector)
        -> QList<decltype(selector(*container.begin()))> {
        // using TItem = std::decay_t<decltype(selector(*container.begin()))>;
        QList<decltype(selector(*container.begin()))> result;
        std::transform(container.begin(), container.end(), std::back_inserter(result), selector);
        return result;
    }

}

#endif // LINQ_H
