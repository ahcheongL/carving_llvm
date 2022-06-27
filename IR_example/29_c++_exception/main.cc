#include <utility>
#include <string>
#include <exception>

int foo (int a) {
    if (a > 0) {
        throw std::exception();
    }
    return 0;
}

int main(int argc, char * argv[])
{
  int fooret = 0;

  try {
    fooret = foo(243);
  } catch (std::exception & e) {
    return (long) e.what();
  }

  return fooret;
}