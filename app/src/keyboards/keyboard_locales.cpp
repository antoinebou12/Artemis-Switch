//
//  keyboard_locales.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 09.01.2022.
//

#include "keyboard_view.hpp"

void KeyboardView::createLocales() {
    locales.clear();
    locales.reserve(10);
    locales.push_back(KeyboardLocale{
        .name = "English",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"0", ")"}, {"1", "!"}, {"2", "@"}, {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},
            {"7", "&"}, {"8", "*"}, {"9", "("}, {"a", "A"}, {"b", "B"}, {"c", "C"}, {"d", "D"}, {"e", "E"}, {"f", "F"}, {"g", "G"},
            {"h", "H"}, {"i", "I"}, {"j", "J"}, {"k", "K"}, {"l", "L"}, {"m", "M"}, {"n", "N"}, {"o", "O"}, {"p", "P"}, {"q", "Q"},
            {"r", "R"}, {"s", "S"}, {"t", "T"}, {"u", "U"}, {"v", "V"}, {"w", "W"}, {"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"Return", "Return"}, {"Space", "Space"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {".", ">"}, {",", "<"}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {";", ":"}, {"/", "?"}, {"`", "~"}, {"[", "{"}, {"\\", "|"}, {"]", "}"}, {"'", "\""}, {"-", "_"}, {"=", "+"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        }
    });

    locales.push_back(KeyboardLocale{
        .name = "Русский",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"0", ")"}, {"1", "!"}, {"2", "\""}, {"3", "№"}, {"4", ";"}, {"5", "%"}, {"6", ":"},
            {"7", "?"}, {"8", "*"}, {"9", "("}, {"ф", "Ф"}, {"и", "И"}, {"с", "С"}, {"в", "В"}, {"у", "У"}, {"а", "А"}, {"п", "П"},
            {"р", "Р"}, {"ш", "Ш"}, {"о", "О"}, {"л", "Л"}, {"д", "Д"}, {"ь", "Ь"}, {"т", "Т"}, {"щ", "Щ"}, {"з", "З"}, {"й", "Й"},
            {"к", "К"}, {"ы", "Ы"}, {"е", "Е"}, {"г", "Г"}, {"м", "М"}, {"ц", "Ц"}, {"ч", "Ч"}, {"н", "Н"}, {"я", "Я"}, {"Ввод", "Ввод"}, {"Пробел", "Пробел"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {"ю", "Ю"}, {"б", "Б"}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {"ж", "Ж"}, {".", ","}, {"ё", "Ё"}, {"х", "Х"}, {"\\", "/"}, {"ъ", "Ъ"}, {"э", "Э"}, {"-", "_"}, {"=", "+"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        }
    });

    locales.push_back(KeyboardLocale{
        .name = "Français",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"à", "0"}, {"&", "1"}, {"é", "2"}, {"\"", "3"}, {"'", "4"}, {"(", "5"}, {"-", "6"},
            {"è", "7"}, {"_", "8"}, {"ç", "9"}, {"a", "A"}, {"b", "B"}, {"c", "C"}, {"d", "D"}, {"e", "E"}, {"f", "F"}, {"g", "G"},
            {"h", "H"}, {"i", "I"}, {"j", "J"}, {"k", "K"}, {"l", "L"}, {"m", "M"}, {"n", "N"}, {"o", "O"}, {"p", "P"}, {"q", "Q"},
            {"r", "R"}, {"s", "S"}, {"t", "T"}, {"u", "U"}, {"v", "V"}, {"w", "W"}, {"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"Return", "Return"}, {"Space", "Space"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {",", "?"}, {";", "."}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {"!", "§"}, {":", "/"}, {"²", "²"}, {"ù", "%"}, {"*", "μ"}, {"=", "+"}, {")", "°"}, {"^", "¨"}, {"$", "£"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        },
        .keyMapper = {{VK_OEM_MINUS, VK_OEM_7}, {VK_OEM_PLUS, VK_OEM_6},
                      {VK_KEY_Q, VK_KEY_A}, {VK_KEY_W, VK_KEY_Z}, {VK_OEM_4, VK_OEM_MINUS}, {VK_OEM_6, VK_OEM_PLUS},
                      {VK_KEY_A, VK_KEY_Q}, {VK_OEM_1, VK_KEY_M}, {VK_OEM_7, VK_OEM_4},
                      {VK_KEY_Z, VK_KEY_W}, {VK_KEY_M, VK_OEM_PERIOD}, {VK_OEM_PERIOD, VK_OEM_2}, {VK_OEM_2, VK_OEM_1}}
    });

    locales.push_back(KeyboardLocale{
        .name = "Deutsch",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"0", "="}, {"1", "!"}, {"2", "\""}, {"3", "§"}, {"4", "$"}, {"5", "%"}, {"6", "&"},
            {"7", "/"}, {"8", "("}, {"9", ")"}, {"a", "A"}, {"b", "B"}, {"c", "C"}, {"d", "D"}, {"e", "E"}, {"f", "F"}, {"g", "G"},
            {"h", "H"}, {"i", "I"}, {"j", "J"}, {"k", "K"}, {"l", "L"}, {"m", "M"}, {"n", "N"}, {"o", "O"}, {"p", "P"}, {"q", "Q"},
            {"r", "R"}, {"s", "S"}, {"t", "T"}, {"u", "U"}, {"v", "V"}, {"w", "W"}, {"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"Return", "Return"}, {"Space", "Space"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {",", ";"}, {".", ":"}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {"ö", "Ö"}, {"-", "_"}, {"^", "°"}, {"ü", "Ü"}, {"#", "'"}, {"+", "*"}, {"ä", "Ä"}, {"ß", "?"}, {"´", "`"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        },
        .keyMapper = {{VK_KEY_Y, VK_KEY_Z}, {VK_KEY_Z, VK_KEY_Y}}
    });

    locales.push_back(KeyboardLocale{
        .name = "Español",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"0", "="}, {"1", "!"}, {"2", "\""}, {"3", "·"}, {"4", "$"}, {"5", "%"}, {"6", "&"},
            {"7", "/"}, {"8", "("}, {"9", ")"}, {"a", "A"}, {"b", "B"}, {"c", "C"}, {"d", "D"}, {"e", "E"}, {"f", "F"}, {"g", "G"},
            {"h", "H"}, {"i", "I"}, {"j", "J"}, {"k", "K"}, {"l", "L"}, {"m", "M"}, {"n", "N"}, {"o", "O"}, {"p", "P"}, {"q", "Q"},
            {"r", "R"}, {"s", "S"}, {"t", "T"}, {"u", "U"}, {"v", "V"}, {"w", "W"}, {"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"Return", "Return"}, {"Space", "Space"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {",", ";"}, {".", ":"}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {"ñ", "Ñ"}, {"-", "_"}, {"º", "ª"}, {"`", "^"}, {"+", "*"}, {"ç", "Ç"}, {"´", "¨"}, {"'", "?"}, {"¡", "¿"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        },
        .keyMapper = {}
    });

    locales.push_back(KeyboardLocale{
        .name = "한국어 (2벌식)",
        .localization = {
            {"Remove", "Remove"}, {"Esc", "Esc"}, {"0", ")"}, {"1", "!"}, {"2", "@"}, {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},
            {"7", "&"}, {"8", "*"}, {"9", "("}, {"a", "A"}, {"b", "B"}, {"c", "C"}, {"d", "D"}, {"e", "E"}, {"f", "F"}, {"g", "G"},
            {"h", "H"}, {"i", "I"}, {"j", "J"}, {"k", "K"}, {"l", "L"}, {"m", "M"}, {"n", "N"}, {"o", "O"}, {"p", "P"}, {"q", "Q"},
            {"r", "R"}, {"s", "S"}, {"t", "T"}, {"u", "U"}, {"v", "V"}, {"w", "W"}, {"x", "X"}, {"y", "Y"}, {"z", "Z"}, {"Return", "Return"}, {"Space", "Space"},
            {"Ctrl", "Ctrl"}, {"Alt", "Alt"}, {"Shift", "Shift"}, {"Win", "Win"}, {".", ">"}, {",", "<"}, {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"},
            {"F5", "F5"}, {"F6", "F6"}, {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"}, {"Tab", "Tab"}, {"Delete", "Delete"},
            {";", ":"}, {"/", "?"}, {"`", "~"}, {"[", "{"}, {"\\", "|"}, {"]", "}"}, {"'", "\""}, {"-", "_"}, {"=", "+"}, {"\u2193", "\u2193"},
            {"\u2190", "\u2190"}, {"\u2192", "\u2192"}, {"\u2191", "\u2191"}, {"CapsLock", "CapsLock"},
        },
        .keyMapper = {}
    });

    // Standard Korean 2-set (Dubeolsik) labels on QWERTY physical keys.
    // See XITRIX/Moonlight-Switch#283.
    {
        auto& korean = locales.back();
        korean.localization[VK_KEY_Q][0] = "ㅂ";
        korean.localization[VK_KEY_Q][1] = "ㅃ";
        korean.localization[VK_KEY_W][0] = "ㅈ";
        korean.localization[VK_KEY_W][1] = "ㅉ";
        korean.localization[VK_KEY_E][0] = "ㄷ";
        korean.localization[VK_KEY_E][1] = "ㄸ";
        korean.localization[VK_KEY_R][0] = "ㄱ";
        korean.localization[VK_KEY_R][1] = "ㄲ";
        korean.localization[VK_KEY_T][0] = "ㅅ";
        korean.localization[VK_KEY_T][1] = "ㅆ";
        korean.localization[VK_KEY_Y][0] = "ㅛ";
        korean.localization[VK_KEY_Y][1] = "ㅛ";
        korean.localization[VK_KEY_U][0] = "ㅕ";
        korean.localization[VK_KEY_U][1] = "ㅕ";
        korean.localization[VK_KEY_I][0] = "ㅑ";
        korean.localization[VK_KEY_I][1] = "ㅑ";
        korean.localization[VK_KEY_O][0] = "ㅐ";
        korean.localization[VK_KEY_O][1] = "ㅒ";
        korean.localization[VK_KEY_P][0] = "ㅔ";
        korean.localization[VK_KEY_P][1] = "ㅖ";
        korean.localization[VK_KEY_A][0] = "ㅁ";
        korean.localization[VK_KEY_A][1] = "ㅁ";
        korean.localization[VK_KEY_S][0] = "ㄴ";
        korean.localization[VK_KEY_S][1] = "ㄴ";
        korean.localization[VK_KEY_D][0] = "ㅇ";
        korean.localization[VK_KEY_D][1] = "ㅇ";
        korean.localization[VK_KEY_F][0] = "ㄹ";
        korean.localization[VK_KEY_F][1] = "ㄹ";
        korean.localization[VK_KEY_G][0] = "ㅎ";
        korean.localization[VK_KEY_G][1] = "ㅎ";
        korean.localization[VK_KEY_H][0] = "ㅗ";
        korean.localization[VK_KEY_H][1] = "ㅗ";
        korean.localization[VK_KEY_J][0] = "ㅓ";
        korean.localization[VK_KEY_J][1] = "ㅓ";
        korean.localization[VK_KEY_K][0] = "ㅏ";
        korean.localization[VK_KEY_K][1] = "ㅏ";
        korean.localization[VK_KEY_L][0] = "ㅣ";
        korean.localization[VK_KEY_L][1] = "ㅣ";
        korean.localization[VK_KEY_Z][0] = "ㅋ";
        korean.localization[VK_KEY_Z][1] = "ㅋ";
        korean.localization[VK_KEY_X][0] = "ㅌ";
        korean.localization[VK_KEY_X][1] = "ㅌ";
        korean.localization[VK_KEY_C][0] = "ㅊ";
        korean.localization[VK_KEY_C][1] = "ㅊ";
        korean.localization[VK_KEY_V][0] = "ㅍ";
        korean.localization[VK_KEY_V][1] = "ㅍ";
        korean.localization[VK_KEY_B][0] = "ㅠ";
        korean.localization[VK_KEY_B][1] = "ㅠ";
        korean.localization[VK_KEY_N][0] = "ㅜ";
        korean.localization[VK_KEY_N][1] = "ㅜ";
        korean.localization[VK_KEY_M][0] = "ㅡ";
        korean.localization[VK_KEY_M][1] = "ㅡ";
    }

    KeyboardLocale englishUk = locales.front();
    englishUk.name = "English (UK)";
    englishUk.localization[VK_KEY_2][1] = "\"";
    englishUk.localization[VK_KEY_3][1] = "£";
    englishUk.localization[VK_OEM_3][0] = "`";
    englishUk.localization[VK_OEM_3][1] = "¬";
    locales.push_back(std::move(englishUk));

    KeyboardLocale portugueseBrazil = locales.front();
    portugueseBrazil.name = "Português (Brasil)";
    portugueseBrazil.localization[VK_OEM_1][0] = "ç";
    portugueseBrazil.localization[VK_OEM_1][1] = "Ç";
    portugueseBrazil.localization[VK_OEM_7][0] = "~";
    portugueseBrazil.localization[VK_OEM_7][1] = "^";
    locales.push_back(std::move(portugueseBrazil));

    KeyboardLocale japaneseJis = locales.front();
    japaneseJis.name = "日本語 (JIS / host IME)";
    japaneseJis.localization[VK_OEM_3][0] = "半/全";
    japaneseJis.localization[VK_OEM_3][1] = "漢字";
    japaneseJis.localization[VK_OEM_7][0] = ":";
    japaneseJis.localization[VK_OEM_7][1] = "*";
    locales.push_back(std::move(japaneseJis));

    KeyboardLocale chineseIme = locales.front();
    chineseIme.name = "中文 (host IME)";
    chineseIme.localization[VK_SPACE][0] = "空格";
    chineseIme.localization[VK_RETURN][0] = "回车";
    locales.push_back(std::move(chineseIme));

    applyIsoAndAltGrLabels();
}

// The ISO key left of Z and the AltGr layer are the two things a US-shaped
// layout table cannot express, so they are filled in per locale here rather
// than widening every initializer above.
void KeyboardView::applyIsoAndAltGrLabels() {
    for (auto& locale : locales) {
        // Universal ISO legend; hosts on US-ANSI simply have no such key.
        locale.localization[VK_OEM_102][KEYBOARD_LABEL_BASE] = "<";
        locale.localization[VK_OEM_102][KEYBOARD_LABEL_SHIFT] = ">";
    }

    const auto find = [](const char* name) -> KeyboardLocale* {
        for (auto& locale : locales)
            if (locale.name == name)
                return &locale;
        return nullptr;
    };

    if (auto* german = find("Deutsch")) {
        // de-de AltGr layer, including the glyphs that were unreachable before:
        // | (AltGr+<), @ (AltGr+Q), \ (AltGr+ß), ~ (AltGr++).
        german->localization[VK_OEM_102][KEYBOARD_LABEL_ALTGR] = "|";
        german->localization[VK_KEY_Q][KEYBOARD_LABEL_ALTGR] = "@";
        german->localization[VK_OEM_MINUS][KEYBOARD_LABEL_ALTGR] = "\\";
        german->localization[VK_OEM_PLUS][KEYBOARD_LABEL_ALTGR] = "~";
        german->localization[VK_KEY_E][KEYBOARD_LABEL_ALTGR] = "€";
        german->localization[VK_KEY_7][KEYBOARD_LABEL_ALTGR] = "{";
        german->localization[VK_KEY_8][KEYBOARD_LABEL_ALTGR] = "[";
        german->localization[VK_KEY_9][KEYBOARD_LABEL_ALTGR] = "]";
        german->localization[VK_KEY_0][KEYBOARD_LABEL_ALTGR] = "}";
        german->localization[VK_KEY_M][KEYBOARD_LABEL_ALTGR] = "µ";
    }

    if (auto* french = find("Français")) {
        french->localization[VK_KEY_E][KEYBOARD_LABEL_ALTGR] = "€";
        french->localization[VK_KEY_0][KEYBOARD_LABEL_ALTGR] = "@";
        french->localization[VK_KEY_3][KEYBOARD_LABEL_ALTGR] = "#";
        french->localization[VK_KEY_4][KEYBOARD_LABEL_ALTGR] = "{";
        french->localization[VK_KEY_5][KEYBOARD_LABEL_ALTGR] = "[";
        french->localization[VK_KEY_6][KEYBOARD_LABEL_ALTGR] = "|";
        french->localization[VK_KEY_8][KEYBOARD_LABEL_ALTGR] = "\\";
        french->localization[VK_KEY_9][KEYBOARD_LABEL_ALTGR] = "^";
        french->localization[VK_OEM_MINUS][KEYBOARD_LABEL_ALTGR] = "]";
        french->localization[VK_OEM_PLUS][KEYBOARD_LABEL_ALTGR] = "}";
    }

    if (auto* spanish = find("Español")) {
        spanish->localization[VK_OEM_102][KEYBOARD_LABEL_ALTGR] = "|";
        spanish->localization[VK_KEY_E][KEYBOARD_LABEL_ALTGR] = "€";
        spanish->localization[VK_KEY_1][KEYBOARD_LABEL_ALTGR] = "|";
        spanish->localization[VK_KEY_2][KEYBOARD_LABEL_ALTGR] = "@";
        spanish->localization[VK_KEY_3][KEYBOARD_LABEL_ALTGR] = "#";
        spanish->localization[VK_OEM_3][KEYBOARD_LABEL_ALTGR] = "\\";
    }
}
