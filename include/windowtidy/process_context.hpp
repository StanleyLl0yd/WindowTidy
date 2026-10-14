#pragma once
#include <windows.h>
#include <vector>

namespace wt {
enum class AccountRelation { Unknown, SameUser, DifferentUser };
enum class IntegrityRelation { Unknown, NotHigher, Higher };

struct WindowProcessContext {
    AccountRelation account = AccountRelation::Unknown;
    IntegrityRelation integrity = IntegrityRelation::Unknown;
};

inline bool ReadTokenBytes(HANDLE token, TOKEN_INFORMATION_CLASS kind,
                           std::vector<BYTE>& buffer) {
    DWORD size = 0;
    GetTokenInformation(token, kind, nullptr, 0, &size);
    if (size == 0 || size > 8192) return false;
    buffer.resize(size);
    return GetTokenInformation(token, kind, buffer.data(), size, &size) != FALSE;
}

inline DWORD TokenIntegrityRid(const std::vector<BYTE>& buffer) {
    if (buffer.size() < sizeof(TOKEN_MANDATORY_LABEL)) return 0;
    const auto* info = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
    const PSID sid = info->Label.Sid;
    if (!sid || !IsValidSid(sid)) return 0;
    const PUCHAR count = GetSidSubAuthorityCount(sid);
    return count && *count ? *GetSidSubAuthority(sid, *count - 1) : 0;
}

// Read-only, on-demand token comparisons. Never returns or saves user SIDs/names.
inline WindowProcessContext InspectWindowProcess(DWORD pid) {
    WindowProcessContext result;
    if (pid == 0) return result;
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return result;

    HANDLE targetToken{};
    const BOOL targetOpened = OpenProcessToken(process, TOKEN_QUERY, &targetToken);
    CloseHandle(process);
    if (!targetOpened) return result;

    HANDLE ownToken{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ownToken)) {
        CloseHandle(targetToken);
        return result;
    }
    std::vector<BYTE> ownUser, targetUser, ownLevel, targetLevel;
    if (ReadTokenBytes(ownToken, TokenUser, ownUser) &&
        ReadTokenBytes(targetToken, TokenUser, targetUser)) {
        const auto* mine = reinterpret_cast<const TOKEN_USER*>(ownUser.data());
        const auto* theirs = reinterpret_cast<const TOKEN_USER*>(targetUser.data());
        if (mine->User.Sid && theirs->User.Sid &&
            IsValidSid(mine->User.Sid) && IsValidSid(theirs->User.Sid)) {
            result.account = EqualSid(mine->User.Sid, theirs->User.Sid)
                ? AccountRelation::SameUser : AccountRelation::DifferentUser;
        }
    }
    if (ReadTokenBytes(ownToken, TokenIntegrityLevel, ownLevel) &&
        ReadTokenBytes(targetToken, TokenIntegrityLevel, targetLevel)) {
        const DWORD mine = TokenIntegrityRid(ownLevel);
        const DWORD theirs = TokenIntegrityRid(targetLevel);
        if (mine != 0 && theirs != 0) {
            result.integrity = theirs > mine
                ? IntegrityRelation::Higher : IntegrityRelation::NotHigher;
        }
    }
    CloseHandle(ownToken);
    CloseHandle(targetToken);
    return result;
}
} // namespace wt
