#pragma once
#include <string>
#include <string_view>

namespace wt {
inline std::wstring SafeMenuLabel(std::wstring_view title) {
    std::wstring result;
    result.reserve(title.size());
    for (wchar_t c : title) {
        if (c == L'&') {
            result += L"&&";
        } else {
            result += c < L' ' ? L' ' : c;
        }
    }
    return result;
}
} // namespace wt
