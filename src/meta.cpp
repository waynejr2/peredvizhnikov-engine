/*
 *  This file is part of Peredvizhnikov Engine
 *  Copyright (C) 2023 Eduard Permyakov 
 *
 *  Peredvizhnikov Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Peredvizhnikov Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

export module meta;

import <type_traits>;
import <utility>;
import <tuple>;
import <iostream>;

namespace pe{

/*
 * Like std::integer_sequence, but not necessarily starting 
 * at zero.
 */
export
template<std::size_t... Is> struct seq
{
    typedef seq<Is...> type;
};

export
template<std::size_t Count, std::size_t Begin, std::size_t... Is>
struct make_seq : make_seq<Count-1, Begin+1, Is..., Begin> {};

template<std::size_t Begin, std::size_t... Is>
struct make_seq<0, Begin, Is...> : seq<Is...> {};

/*
 * Helper to extract a specific subrange from a tuple.
 */
export
template<std::size_t... Is, typename Tuple>
auto extract_tuple(seq<Is...>, Tuple& tup) 
{
    return std::forward_as_tuple(std::get<Is>(tup)...);
}
/*
 * Helper to query for the return type and argument types of 
 * various different callables: lambdas, functors, and functions.
 */
export
template <typename T>
struct function_traits : public function_traits<decltype(&T::operator())>
{};

export
template <typename T, typename R, typename... Args>
struct function_traits<R(T::*)(Args...)>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

export
template <typename T, typename R, typename... Args>
struct function_traits<R(T::*)(Args...) const>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

export
template <typename T, typename R, typename... Args>
struct function_traits<R(T::*)(Args...) const noexcept>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

export
template <typename R, typename... Args>
struct function_traits<R(*)(Args...)>
{
    using return_type = R;
    using args_type = std::tuple<Args...>;
};

/* Check if a given type is an instance of a specific template.
 */

export
template <class, template <class...> class>
struct is_template_instance : public std::false_type{};

export
template <class... Args, template <class...> class T>
struct is_template_instance<T<Args...>, T> : public std::true_type{};

export
template <std::size_t Start, std::size_t End, std::size_t Inc, typename F>
constexpr inline void constexpr_for(F&& lambda)
{
    if constexpr (Start < End) {
        lambda.template operator()<Start>();
        constexpr_for<Start + Inc, End, Inc>(lambda);
    }
}

}; //namespace pe

template <std::size_t... Is>
std::ostream& operator<<(std::ostream& stream, pe::seq<Is...> sequence)
{
    const char *sep = "";
    (((stream << sep << Is), sep = ", "), ...);
    return stream;
}

