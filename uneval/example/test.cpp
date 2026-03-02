// test.cpp
template <typename T>
using unevaluated = T;

int f(auto x, int y) { return x() + y; }

int g() {
  int a = 40;
  return f([&]{ return (a + 2); }, 1);
}
