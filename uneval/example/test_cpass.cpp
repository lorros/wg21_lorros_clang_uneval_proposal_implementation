#include <iostream>
#include <type_traits>

template <typename T>
using cpass = T;

template<typename T>
struct my_cpassable_monad
{
    T i;

    my_cpassable_monad(T i) : i(i) {}

    template<typename F>
    cpass<my_cpassable_monad<std::invoke_result_t<F, T>>> operator()(F&& f) const
    {
        if constexpr(std::is_same_v<T, int>) {
            if (!i) {
                std::cout << "Warning: i == 0" << std::endl;
                if constexpr(std::is_same_v<std::invoke_result_t<F, T>, std::ostream&>) {
                    return my_cpassable_monad<std::invoke_result_t<F, T>>(std::forward<F>(f)("0 or INF"));
                } else {
                    return my_cpassable_monad<std::invoke_result_t<F, T>>({});
                }
            }
        }
        return my_cpassable_monad<std::invoke_result_t<F, T>>(std::forward<F>(f)(i));
    }

    operator T() const { return i; }  // not called normally

    static cpass<my_cpassable_monad<T>> ret(T i) { return my_cpassable_monad<T>(i); }
};


int f(int x, int y) { return x / y; }


int main()
{
    my_cpassable_monad<int>::ret(0)([&](auto&& cval) -> decltype(f(2, std::forward<decltype(cval)>(cval))) { return f(2, std::forward<decltype(cval)>(cval)); })([&](auto&& cval) -> decltype(f(1, std::forward<decltype(cval)>(cval))) { return f(1, std::forward<decltype(cval)>(cval)); })([&](auto&& cval) -> decltype(std::cout << std::forward<decltype(cval)>(cval)) { return std::cout << std::forward<decltype(cval)>(cval); })([&](auto&& cval) -> decltype(std::forward<decltype(cval)>(cval) << " * ") { return std::forward<decltype(cval)>(cval) << " * "; })([&](auto&& cval) -> decltype(std::forward<decltype(cval)>(cval) << std::endl) { return std::forward<decltype(cval)>(cval) << std::endl; });
}
