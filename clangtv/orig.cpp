#include <iostream>

template<typename T>
using unevaluated = T;

bool f(bool b)
{
    std::cout << "f:" << b << std::endl;
    return b;
}

bool either(bool a, unevaluated<bool> b)
{
    if (a) return true;
    return b;
}

int main()
{
    std::cout << either(f(true), f(false)) << std::endl;
}
