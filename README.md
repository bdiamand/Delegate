# Delegate
Three C++ fixed-size delegate classes, one small and fast, and two slightly larger for non-movable and non-copyable objects.

The fastest version uses a single pointer and however much extra space you want for captures.  The more full featured variants use one more pointer.

All three are fixed size (never allocate), can't be in an uncallable state, and are straightforward to use.

Depends on https://github.com/catchorg/Catch2, but only for the unit tests.  See the unit tests for examples for non-POD types and move semantics (e.g. to capture things like unique_ptr).

Note: The code should work fine for C++11, although for lambdas with move-on-capture, at least C++14 is needed.

Two examples uses:

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
    Delegate::FuncTrivial<int, int> f = &func;
    ...
    f(12);
}

//lambda with POD capture.  To support non-POD captures (many projects never need this), use one of FuncNonMove or FuncNonCopy.
{
    double d = 99.5;
    double *something = &d;
    Delegate::FuncTrivial<double *, double> g = [=](const double d) 
    {
        *something += d;

        return something;
    };
    ...
    g(12.345);
}
```
