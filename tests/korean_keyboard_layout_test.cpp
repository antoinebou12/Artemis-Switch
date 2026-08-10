#include <cassert>
#include <iostream>
#include <string>

// Mirrors the Standard Korean 2-set (Dubeolsik) QWERTY overlay used by
// KeyboardView::createLocales() for XITRIX/Moonlight-Switch#283.

struct HangulKey {
    const char* unshifted;
    const char* shifted;
};

HangulKey dubeolsikForQwerty(char key) {
    switch (key) {
    case 'q':
        return {"ㅂ", "ㅃ"};
    case 'w':
        return {"ㅈ", "ㅉ"};
    case 'e':
        return {"ㄷ", "ㄸ"};
    case 'r':
        return {"ㄱ", "ㄲ"};
    case 't':
        return {"ㅅ", "ㅆ"};
    case 'y':
        return {"ㅛ", "ㅛ"};
    case 'u':
        return {"ㅕ", "ㅕ"};
    case 'i':
        return {"ㅑ", "ㅑ"};
    case 'o':
        return {"ㅐ", "ㅒ"};
    case 'p':
        return {"ㅔ", "ㅖ"};
    case 'a':
        return {"ㅁ", "ㅁ"};
    case 's':
        return {"ㄴ", "ㄴ"};
    case 'd':
        return {"ㅇ", "ㅇ"};
    case 'f':
        return {"ㄹ", "ㄹ"};
    case 'g':
        return {"ㅎ", "ㅎ"};
    case 'h':
        return {"ㅗ", "ㅗ"};
    case 'j':
        return {"ㅓ", "ㅓ"};
    case 'k':
        return {"ㅏ", "ㅏ"};
    case 'l':
        return {"ㅣ", "ㅣ"};
    case 'z':
        return {"ㅋ", "ㅋ"};
    case 'x':
        return {"ㅌ", "ㅌ"};
    case 'c':
        return {"ㅊ", "ㅊ"};
    case 'v':
        return {"ㅍ", "ㅍ"};
    case 'b':
        return {"ㅠ", "ㅠ"};
    case 'n':
        return {"ㅜ", "ㅜ"};
    case 'm':
        return {"ㅡ", "ㅡ"};
    default:
        return {"", ""};
    }
}

int main() {
    // Cases reported as wrong in Moonlight-Switch#283.
    assert(std::string(dubeolsikForQwerty('w').unshifted) == "ㅈ");
    assert(std::string(dubeolsikForQwerty('r').unshifted) == "ㄱ");
    assert(std::string(dubeolsikForQwerty('t').unshifted) == "ㅅ");
    assert(std::string(dubeolsikForQwerty('y').unshifted) == "ㅛ");
    assert(std::string(dubeolsikForQwerty('u').unshifted) == "ㅕ");
    assert(std::string(dubeolsikForQwerty('z').unshifted) == "ㅋ");
    assert(std::string(dubeolsikForQwerty('x').unshifted) == "ㅌ");
    assert(std::string(dubeolsikForQwerty('v').unshifted) == "ㅍ");

    std::cout << "korean_keyboard_layout_test ok\n";
    return 0;
}
