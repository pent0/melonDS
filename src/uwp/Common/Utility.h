#pragma once

#include <string>

namespace uwp {

Platform::String^ CharToString(const char * char_array) {
    std::string s_str = std::string(char_array);
    std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
    const wchar_t* w_char = wid_str.c_str();
    return ref new Platform::String(w_char);
}

}
