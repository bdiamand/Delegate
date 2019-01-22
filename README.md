# Delegate
Fixed-size C++ delegates, which are efficient alternatives to std::function.

There are two variants, one which is for non-movable objects (the 99% case) and one for non-copyable objects (the 1% case).

Both variants are fixed size (never allocate), can't be in an uncallable state, and are straightforward to use.

Depends on https://github.com/catchorg/Catch2 only for the unit tests, the delegate.h file can be included and compiled by any compliant C++14 compiler.

See the unit tests for more complete examples, (e.g. to capture things like unique_ptr), but a couple of simple examples:

```c++
...
#include "delegate.h
...

int func(int i)
{
    return 17 + i;
}

//simple function delegate, no captures
{
    delegate::Delegate<int, int> f = &func;
    ...
    f(12);
}

//lambda with POD capture.  To support non-POD captures (many projects never need this), use one of FuncNonMove or FuncNonCopy.
{
    double d = 99.5;
    double *something = &d;
    delegate::Delegate<double *, double> g = [=](const double d) 
    {
        *something += d;

        return something;
    };
    ...
    g(12.345);
}
```
