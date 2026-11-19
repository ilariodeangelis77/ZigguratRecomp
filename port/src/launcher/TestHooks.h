// Test-only launcher hooks shared by App and the screen classes.
#pragma once

namespace zl {
// Page index to apply once on the first draw of a tab with a page selector (-1 = none). Set by --page N.
extern int gStartPage;
inline void takeStartPage(int& page) {
    if (gStartPage >= 0) {
        page = gStartPage;
        gStartPage = -1;
    }
}
}  // namespace zl
