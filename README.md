# Delegate
Fixed-size C++ delegates - efficient alternatives to std::function.

Some performance benchmarks against std::function and SG14 (among others) here: https://github.com/jamboree/CxxFunctionBenchmark.

As of the time of this comment, Delegate is the fastest of the implementations.  It occupies 8 bytes plus capture size on 32-bit systems, and 8 additional bytes on 64-bit systems.

There are two variants, one for capturing non-movable objects (the 99% case) and one for capturing non-copyable objects (the 1% case).

It depends on https://github.com/catchorg/Catch2 only for the unit tests; the delegate.h file can be included and compiled by any compliant C++14 compiler.

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

//lambda with POD capture, but non-POD also works correctly.
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
