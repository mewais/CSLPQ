#ifndef __CSLPQ_CONCEPT_HPP__
#define __CSLPQ_CONCEPT_HPP__

#include <type_traits>
#include <sstream>

namespace CSLPQ
{
    template <class T, class EqualTo>
    struct is_comparable_impl 
    {
        template <class U, class V>
        static auto test(U*) -> decltype(
                std::declval<U>() == std::declval<V>() &&
                std::declval<U>() != std::declval<V>() &&
                std::declval<U>() < std::declval<V>() &&
                std::declval<U>() > std::declval<V>() &&
                std::declval<U>() <= std::declval<V>() &&
                std::declval<U>() >= std::declval<V>());

        template <class, class>
        static auto test(...) -> std::false_type;

        using type = typename std::is_same<bool, decltype(test<T, EqualTo>(0))>::type;
    };

    template <class T, class EqualTo = T>
    struct is_comparable : is_comparable_impl<T, EqualTo>::type {};

    template <class T>
    struct is_printable_impl
    {
        template <class U>
        static auto test(U*) -> decltype(std::declval<std::stringstream&>() << std::declval<U>(), std::true_type());

        template <class>
        static auto test(...) -> std::false_type;

        using type = decltype(test<T>(0));
    };

    template <class T>
    struct is_printable : is_printable_impl<T>::type {};
}

#endif //__CSLPQ_CONCEPT_HPP__
