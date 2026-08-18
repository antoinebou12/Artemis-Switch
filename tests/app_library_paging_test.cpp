#include "AppLibraryPaging.hpp"

#include <cassert>

int main() {
    using namespace artemis::ui;

    static_assert(AppLibraryColumns == 5);
    static_assert(AppLibraryPageSize == 25);
    assert(appLibraryNameKey("  Game Name  ") == "game name");
    assert(appLibraryNameKey("GAME\t  NAME") == "game name");
    assert(appLibraryNameKey("Different") != appLibraryNameKey("Game Name"));

    auto empty = appLibraryPageWindow(0, AppLibraryPageSize);
    assert(empty.displayed == 0);
    assert(!empty.hasMore);

    auto exact = appLibraryPageWindow(25, AppLibraryPageSize);
    assert(exact.displayed == 25);
    assert(!exact.hasMore);

    auto large = appLibraryPageWindow(91, AppLibraryPageSize);
    assert(large.displayed == 25);
    assert(large.hasMore);
    assert(nextAppLibraryLimit(91, 25) == 50);
    assert(nextAppLibraryLimit(91, 50) == 75);
    assert(nextAppLibraryLimit(91, 75) == 91);

    auto filtered = appLibraryPageWindow(7, 56);
    assert(filtered.displayed == 7);
    assert(!filtered.hasMore);

    return 0;
}
