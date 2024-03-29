#ifndef __CSLPQ_CONCEPT_HPP__
#define __CSLPQ_CONCEPT_HPP__

#include <concepts>
#include <type_traits>

namespace CSLPQ
{
    template <class T>
    concept Printable = requires(std::stringstream& os, T a)
    {
        os << a;
    };

    template <class T>
    concept OnlyMoveConstructible = std::is_move_constructible_v<T> && !std::is_fundamental_v<T>;

    template <class T>
    concept OnlyCopyConstructible = std::is_copy_constructible_v<T> && !std::is_move_constructible_v<T> &&
                                    !std::is_fundamental_v<T>;

    template <class T>
    concept KeyType = std::totally_ordered<T>;

    template <class T>
    concept ValueType = (std::is_move_constructible_v<T> || std::is_copy_constructible_v<T> ||
                         std::is_default_constructible_v<T> || std::is_fundamental_v<T>);
}

#endif //__CSLPQ_CONCEPT_HPP__
