// Guards the whole unit suite. Almost every test in this directory reports a
// failure through assert(). Release, RelWithDebInfo and MinSizeRel add
// -DNDEBUG, which turns assert(expr) into nothing -- expression and side
// effects included -- so a Release build of the suite "passes" while checking
// nothing. tests/CMakeLists.txt strips NDEBUG from the suite's flags; this
// test fails loudly if that ever regresses or a toolchain injects it again
// (for example through CXXFLAGS).
#include <cassert>
#include <iostream>

int main() {
#if defined(NDEBUG)
    std::cerr << "NDEBUG is defined: assert() is compiled out, so the unit "
                 "tests in this build verify nothing.\n";
    return 1;
#else
    bool evaluated = false;
    assert((evaluated = true)); // must run, side effect included
    if (!evaluated) {
        std::cerr << "assert() did not evaluate its expression.\n";
        return 1;
    }
    return 0;
#endif
}
