#define NOMINMAX

#include <windows.h>
#include <winsvc.h>
#include <shellapi.h>
#include <shlobj.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <lmcons.h>
#include <conio.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <utility>
#include <map>
#include <set>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <cmath>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Version.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(linker, "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
#pragma comment(linker, "/DEPENDENTLOADFLAG:0x800")

using namespace std;

// ============================================================
// НАСТРОЙКИ И ТЕМЫ
// ============================================================

HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);

const WORD C_WHITE = FOREGROUND_RED | FOREGROUND_GREEN |
FOREGROUND_BLUE | FOREGROUND_INTENSITY;
const WORD C_GRAY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
const WORD C_YELLOW = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

WORD THEME_MAIN = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
WORD THEME_TEXT = C_WHITE;
WORD THEME_ACCENT = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
WORD THEME_DIM = FOREGROUND_INTENSITY;
WORD THEME_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;

int  currentTheme = 2;
bool useEmoji = true;
wstring currentUser = L"Пользователь";

struct Theme
{
    wstring name;
    WORD main;
    WORD text;
    WORD accent;
    WORD dim;
    WORD error;
};

vector<Theme> themes =
{
    { L"Красно-белая",
      FOREGROUND_RED | FOREGROUND_INTENSITY, C_WHITE, C_GRAY,
      FOREGROUND_INTENSITY, FOREGROUND_RED | FOREGROUND_INTENSITY },

    { L"Чёрно-белая",
      C_WHITE, C_WHITE, C_GRAY,
      FOREGROUND_INTENSITY, FOREGROUND_RED | FOREGROUND_INTENSITY },

    { L"Чёрно-зелёная",
      FOREGROUND_GREEN | FOREGROUND_INTENSITY, C_WHITE,
      FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
      FOREGROUND_INTENSITY, FOREGROUND_RED | FOREGROUND_INTENSITY },

    { L"Красно-чёрная",
      FOREGROUND_RED | FOREGROUND_INTENSITY, C_WHITE, FOREGROUND_RED,
      FOREGROUND_INTENSITY, FOREGROUND_RED | FOREGROUND_INTENSITY }
};

void applyTheme()
{
    if (currentTheme < 0 || currentTheme >= (int)themes.size())
        currentTheme = 2;

    THEME_MAIN = themes[currentTheme].main;
    THEME_TEXT = themes[currentTheme].text;
    THEME_ACCENT = themes[currentTheme].accent;
    THEME_DIM = themes[currentTheme].dim;
    THEME_ERROR = themes[currentTheme].error;
}

// ============================================================
// КОНСОЛЬ (без system("cls") / system("mode"))
// ============================================================

const int CON_COLS = 100;
const int CON_ROWS = 50;
const int CON_BUFFER_ROWS = 500;

const wchar_t* APP_VERSION = L"1.3.1";
const wchar_t* APP_AUTHOR = L"Kitarno";

int g_margin = 0;
int g_rows = CON_ROWS;

void color(WORD c)
{
    (void)SetConsoleTextAttribute(con, c);
}

void rawWrite(const wchar_t* s, size_t n)
{
    DWORD written = 0;
    if (n)
        (void)WriteConsoleW(con, s, (DWORD)n, &written, nullptr);
}

void eraseEol()
{
    CONSOLE_SCREEN_BUFFER_INFO i{};
    if (!GetConsoleScreenBufferInfo(con, &i))
        return;

    DWORD n = (DWORD)(i.dwSize.X - i.dwCursorPosition.X);
    DWORD w = 0;
    (void)FillConsoleOutputCharacterW(con, L' ', n, i.dwCursorPosition, &w);
}

void p(const wstring& s)
{
    size_t start = 0;

    for (size_t i = 0; i <= s.size(); ++i)
    {
        if (i == s.size() || s[i] == L'\n' || s[i] == L'\r')
        {
            rawWrite(s.c_str() + start, i - start);

            if (i < s.size())
            {
                if (s[i] == L'\n')
                {
                    eraseEol();
                    rawWrite(L"\n", 1);
                }
                else
                {
                    rawWrite(L"\r", 1);
                }

                if (g_margin > 0)
                    rawWrite(wstring((size_t)g_margin, L' ').c_str(), (size_t)g_margin);
            }

            start = i + 1;
        }
    }
}

void gotoxy(int x, int y)
{
    COORD c;
    c.X = (SHORT)(x + g_margin);
    c.Y = (SHORT)y;
    (void)SetConsoleCursorPosition(con, c);
}

COORD cursorPos()
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    (void)GetConsoleScreenBufferInfo(con, &info);
    return info.dwCursorPosition;
}

void showCursor(bool visible)
{
    CONSOLE_CURSOR_INFO ci{};
    if (!GetConsoleCursorInfo(con, &ci))
        return;
    ci.bVisible = visible ? TRUE : FALSE;
    if (ci.dwSize == 0)
        ci.dwSize = 25;
    (void)SetConsoleCursorInfo(con, &ci);
}

void clear()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi{};

    if (!GetConsoleScreenBufferInfo(con, &csbi))
        return;

    DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    DWORD written = 0;
    COORD origin = { 0, 0 };

    (void)FillConsoleOutputCharacterW(con, L' ', cells, origin, &written);
    (void)FillConsoleOutputAttribute(con, csbi.wAttributes, cells, origin, &written);

    SMALL_RECT view;
    view.Left = 0;
    view.Top = 0;
    view.Right = (SHORT)(csbi.srWindow.Right - csbi.srWindow.Left);
    view.Bottom = (SHORT)(csbi.srWindow.Bottom - csbi.srWindow.Top);
    (void)SetConsoleWindowInfo(con, TRUE, &view);

    gotoxy(0, 0);
}

void home()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi{};

    if (!GetConsoleScreenBufferInfo(con, &csbi))
        return;

    if (csbi.srWindow.Top != 0)
    {
        SMALL_RECT view;
        view.Left = 0;
        view.Top = 0;
        view.Right = (SHORT)(csbi.srWindow.Right - csbi.srWindow.Left);
        view.Bottom = (SHORT)(csbi.srWindow.Bottom - csbi.srWindow.Top);
        (void)SetConsoleWindowInfo(con, TRUE, &view);
    }

    gotoxy(0, 0);
}

void eraseBelow()
{
    CONSOLE_SCREEN_BUFFER_INFO i{};

    if (!GetConsoleScreenBufferInfo(con, &i))
        return;

    LONG total = (LONG)i.dwSize.X * (LONG)i.dwSize.Y;
    LONG used = (LONG)i.dwCursorPosition.Y * (LONG)i.dwSize.X + i.dwCursorPosition.X;

    if (used >= total)
        return;

    DWORD w = 0;
    (void)FillConsoleOutputCharacterW(con, L' ', (DWORD)(total - used),
        i.dwCursorPosition, &w);
}

void setupConsole()
{
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;

    if (GetConsoleMode(in, &mode))
    {
        mode &= ~ENABLE_QUICK_EDIT_MODE;
        mode |= ENABLE_EXTENDED_FLAGS;
        (void)SetConsoleMode(in, mode);
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (!GetConsoleScreenBufferInfo(con, &csbi))
        return;

    COORD largest = GetLargestConsoleWindowSize(con);
    int cols = CON_COLS;
    int rows = CON_ROWS;

    if (largest.X > 0) cols = min(cols, (int)largest.X);
    if (largest.Y > 0) rows = min(rows, (int)largest.Y);

    SMALL_RECT tiny = { 0, 0, 1, 1 };
    (void)SetConsoleWindowInfo(con, TRUE, &tiny);

    COORD size;
    size.X = (SHORT)cols;
    size.Y = (SHORT)max(CON_BUFFER_ROWS, rows);
    (void)SetConsoleScreenBufferSize(con, size);

    SMALL_RECT win;
    win.Left = 0;
    win.Top = 0;
    win.Right = (SHORT)(cols - 1);
    win.Bottom = (SHORT)(rows - 1);
    (void)SetConsoleWindowInfo(con, TRUE, &win);

    if (GetConsoleScreenBufferInfo(con, &csbi))
    {
        int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        g_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        g_margin = w > CON_COLS ? (w - CON_COLS) / 2 : 0;
    }
}

void unmountOffline();

BOOL WINAPI ctrlHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT)
        return TRUE;

    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT)
        unmountOffline();

    return FALSE;
}

// ============================================================
// ВВОД (физические клавиши: работает и в русской раскладке)
// ============================================================

enum Key
{
    K_NONE = 0,
    K_ENTER = 13,
    K_ESC = 27,
    K_UP = 1001,
    K_DOWN,
    K_LEFT,
    K_RIGHT,
    K_HOME,
    K_END,
    K_DEL,
    K_PGUP,
    K_PGDN
};

int readKeyCore()
{
    int k = (int)_getwch();

    if (k == 0 || k == 0xE0)
    {
        int c = (int)_getwch();

        switch (c)
        {
        case 72: return K_UP;
        case 80: return K_DOWN;
        case 75: return K_LEFT;
        case 77: return K_RIGHT;
        case 71: return K_HOME;
        case 79: return K_END;
        case 83: return K_DEL;
        case 73: return K_PGUP;
        case 81: return K_PGDN;
        }

        return K_NONE;
    }

    return k;
}

int readKeyRaw()
{
    int k = readKeyCore();

    if (k == K_NONE || k >= K_UP)
        return k;

    if (k >= 0x430 && k <= 0x44F)
        k -= 32;

    if (k >= 0x410 && k <= 0x42F)
    {
        static const wchar_t cy[] =
        {
            0x419, 0x426, 0x423, 0x41A, 0x415, 0x41D, 0x413, 0x428, 0x429,
            0x417, 0x424, 0x42B, 0x412, 0x410, 0x41F, 0x420, 0x41E, 0x41B,
            0x414, 0x42F, 0x427, 0x421, 0x41C, 0x418, 0x422, 0x42C
        };
        static const char la[] = "QWERTYUIOPASDFGHJKLZXCVBNM";

        for (int i = 0; i < 26; ++i)
            if (cy[i] == (wchar_t)k)
                return la[i];

        return k;
    }

    if (k >= 'a' && k <= 'z')
        k -= 32;

    return k;
}

int readKey()
{
    eraseBelow();
    return readKeyRaw();
}

// ============================================================
// ТЕКСТОВЫЕ УТИЛИТЫ
// ============================================================

wstring trimW(const wstring& s)
{
    size_t b = 0;
    size_t e = s.size();

    while (b < e && (s[b] == L' ' || s[b] == L'\t' ||
        s[b] == L'\r' || s[b] == L'\n'))
        ++b;

    while (e > b && (s[e - 1] == L' ' || s[e - 1] == L'\t' ||
        s[e - 1] == L'\r' || s[e - 1] == L'\n'))
        --e;

    return s.substr(b, e - b);
}

wstring lowerW(wstring s)
{
    if (!s.empty())
        CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}

wstring sanitize(const wstring& in)
{
    wstring s = in;

    for (size_t i = 0; i < s.size(); ++i)
    {
        wchar_t c = s[i];

        if (c < 32 || (c >= 0x7F && c <= 0x9F) || c == 0x200E || c == 0x200F ||
            (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069))
            s[i] = L' ';
    }

    return s;
}

wstring clip(const wstring& in, size_t n)
{
    wstring s = sanitize(in);

    if (s.size() <= n)
        return s;
    if (n <= 3)
        return s.substr(0, n);
    return s.substr(0, n - 3) + L"...";
}

wstring padR(const wstring& s, size_t n)
{
    wstring r = clip(s, n);
    if (r.size() < n)
        r.append(n - r.size(), L' ');
    return r;
}

wstring centerIn(const wstring& s, size_t n)
{
    wstring r = clip(s, n);
    size_t total = n - r.size();
    size_t left = total / 2;
    return wstring(left, L' ') + r + wstring(total - left, L' ');
}

wstring fmtGB(ULONGLONG bytes, int precision)
{
    wstringstream ss;
    ss << fixed << setprecision(precision) << bytes / 1073741824.0;
    return ss.str();
}

// ============================================================
// UTF-8
// ============================================================

string utf8(const wstring& s)
{
    if (s.empty())
        return string();

    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
        nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return string();

    string result((size_t)n, '\0');

    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
        &result[0], n, nullptr, nullptr);
    return result;
}

wstring fromUtf8(const string& s)
{
    if (s.empty())
        return wstring();

    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
        nullptr, 0);
    if (n <= 0)
        return L"[Ошибка чтения UTF-8]";

    wstring result((size_t)n, L'\0');

    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(),
        &result[0], n);
    return result;
}

// ============================================================
// ЖУРНАЛ (в %LOCALAPPDATA%, а не в текущей папке)
// ============================================================

wstring logPath()
{
    static wstring path;

    if (!path.empty())
        return path;

    wchar_t base[MAX_PATH] = {};
    wstring dir;

    if (SUCCEEDED(SHGetFolderPathW(nullptr,
        CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
        nullptr, SHGFP_TYPE_CURRENT, base)))
    {
        dir = wstring(base) + L"\\MyScannerRecovery";
    }
    else
    {
        GetTempPathW(MAX_PATH, base);
        dir = wstring(base) + L"MyScannerRecovery";
    }

    (void)CreateDirectoryW(dir.c_str(), nullptr);

    path = dir + L"\\MyScannerRecovery.log";
    return path;
}

void logw(const wstring& message)
{
    SYSTEMTIME t{};
    GetLocalTime(&t);

    wstring clean = message;
    for (size_t i = 0; i < clean.size(); ++i)
        if (clean[i] < 32)
            clean[i] = L' ';

    wchar_t stamp[64] = {};
    swprintf_s(stamp, L"[%02d.%02d.%04d %02d:%02d:%02d] ",
        (int)t.wDay, (int)t.wMonth, (int)t.wYear,
        (int)t.wHour, (int)t.wMinute, (int)t.wSecond);

    string data = utf8(wstring(stamp) + clean + L"\r\n");

    wstring path = logPath();

    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad) &&
        fad.nFileSizeHigh == 0 && fad.nFileSizeLow > 1024u * 1024u)
    {
        (void)MoveFileExW(path.c_str(), (path + L".old").c_str(),
            MOVEFILE_REPLACE_EXISTING);
    }

    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;

    LARGE_INTEGER size{};
    (void)GetFileSizeEx(h, &size);

    DWORD written = 0;

    if (size.QuadPart == 0)
        (void)WriteFile(h, "\xEF\xBB\xBF", 3, &written, nullptr);

    (void)WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(h);
}

vector<wstring> readLogLines()
{
    vector<wstring> lines;

    HANDLE h = CreateFileW(logPath().c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return lines;

    LARGE_INTEGER size{};
    (void)GetFileSizeEx(h, &size);

    if (size.QuadPart > 4 * 1024 * 1024)
    {
        CloseHandle(h);
        return lines;
    }

    string bytes((size_t)size.QuadPart, '\0');
    DWORD got = 0;

    if (!bytes.empty())
        (void)ReadFile(h, &bytes[0], (DWORD)bytes.size(), &got, nullptr);

    CloseHandle(h);
    bytes.resize(got);

    if (bytes.size() >= 3 &&
        (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB &&
        (unsigned char)bytes[2] == 0xBF)
    {
        bytes.erase(0, 3);
    }

    wstring text = fromUtf8(bytes);
    wstring line;

    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\n')
        {
            lines.push_back(line);
            line.clear();
        }
        else if (text[i] != L'\r')
        {
            line.push_back(text[i]);
        }
    }

    if (!line.empty())
        lines.push_back(line);

    return lines;
}

// ============================================================
// АДМИНИСТРАТОР
// ============================================================

bool adminCheck()
{
    BOOL result = FALSE;
    PSID sid = nullptr;

    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid))
    {
        if (!CheckTokenMembership(nullptr, sid, &result))
            result = FALSE;
        FreeSid(sid);
    }

    return result == TRUE;
}

bool admin()
{
    static int cached = -1;

    if (cached < 0)
        cached = adminCheck() ? 1 : 0;

    return cached == 1;
}

bool elevate()
{
    wchar_t path[MAX_PATH] = {};

    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);

    if (!n || n >= MAX_PATH)
        return false;

    HINSTANCE result = ShellExecuteW(nullptr, L"runas", path, nullptr,
        nullptr, SW_SHOWNORMAL);

    return (INT_PTR)result > 32;
}

// ============================================================
// СИСТЕМНЫЕ ПУТИ (учёт WOW64: 32-бит процесс -> Sysnative)
// ============================================================

bool os64()
{
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    return si.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_INTEL;
}

wstring sysDir()
{
    static wstring cached;

    if (!cached.empty())
        return cached;

    wchar_t win[MAX_PATH] = {};
    UINT n = GetSystemWindowsDirectoryW(win, MAX_PATH);

    wstring w(win, n);

    BOOL wow = FALSE;
    (void)IsWow64Process(GetCurrentProcess(), &wow);

    cached = w + (wow ? L"\\Sysnative" : L"\\System32");
    return cached;
}

wstring sysExe(const wstring& name)
{
    wstring path = sysDir() + L"\\" + name;

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        wchar_t win[MAX_PATH] = {};
        UINT n = GetSystemWindowsDirectoryW(win, MAX_PATH);
        path = wstring(win, n) + L"\\System32\\" + name;
    }

    return path;
}

// ============================================================
// РЕЕСТР (безопасное чтение: динамические буферы, гарантированный NUL)
// ============================================================

struct RegVal
{
    bool    found = false;
    DWORD   type = 0;
    wstring str;
    DWORD   num = 0;
};

wstring rootName(HKEY root)
{
    if (root == HKEY_LOCAL_MACHINE) return L"HKLM";
    if (root == HKEY_CURRENT_USER)  return L"HKCU";
    return L"HKEY";
}

bool startsWithNoCase(const wstring& s, const wstring& prefix)
{
    return s.size() >= prefix.size() &&
        _wcsnicmp(s.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool isOwnKey(const wstring& path)
{
    return startsWithNoCase(path, L"Software\\MyScannerRecovery");
}

struct OfflineState
{
    bool    active = false;
    bool    softwareLoaded = false;
    bool    systemLoaded = false;
    bool    userLoaded = false;
    wstring winDir;
    wstring userName;
    wstring controlSet = L"ControlSet001";
};

OfflineState g_off;

const wchar_t* OFF_SOFTWARE = L"MSR_OFF_SOFTWARE";
const wchar_t* OFF_SYSTEM = L"MSR_OFF_SYSTEM";
const wchar_t* OFF_USER = L"MSR_OFF_USER";

struct LiveScope
{
    bool saved;
    LiveScope() : saved(g_off.active) { g_off.active = false; }
    ~LiveScope() { g_off.active = saved; }
};

bool mapKey(HKEY& root, wstring& path, REGSAM& view)
{
    if (!g_off.active || isOwnKey(path))
        return true;

    if (root == HKEY_LOCAL_MACHINE)
    {
        if (startsWithNoCase(path, L"SOFTWARE\\"))
        {
            wstring rest = path.substr(9);

            path = wstring(OFF_SOFTWARE) + L"\\" +
                ((view & KEY_WOW64_32KEY) ? L"Wow6432Node\\" : L"") + rest;
        }
        else if (startsWithNoCase(path, L"SYSTEM\\CurrentControlSet\\"))
        {
            path = wstring(OFF_SYSTEM) + L"\\" + g_off.controlSet + L"\\" +
                path.substr(25);
        }
        else
        {
            return false;
        }
    }
    else if (root == HKEY_CURRENT_USER)
    {
        if (!g_off.userLoaded)
            return false;

        path = wstring(OFF_USER) + L"\\" + path;
    }
    else
    {
        return false;
    }

    root = HKEY_LOCAL_MACHINE;
    view = 0;
    return true;
}

LONG regOpenKey(HKEY root, const wstring& path, REGSAM access, REGSAM view,
    HKEY* out, bool create = false)
{
    HKEY r = root;
    wstring mapped = path;
    REGSAM v = view;

    if (!mapKey(r, mapped, v))
        return ERROR_FILE_NOT_FOUND;

    if (create)
        return RegCreateKeyExW(r, mapped.c_str(), 0, nullptr, 0,
            access | v, nullptr, out, nullptr);

    return RegOpenKeyExW(r, mapped.c_str(), 0, access | v, out);
}

bool regReadRaw(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, DWORD& type, vector<BYTE>& data)
{
    HKEY key = nullptr;

    if (regOpenKey(root, path, KEY_QUERY_VALUE, view, &key) != ERROR_SUCCESS)
        return false;

    DWORD size = 0;
    type = 0;

    LONG r = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size);

    if (r != ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return false;
    }

    data.assign((size_t)size + 4, 0);

    DWORD got = size;

    r = RegQueryValueExW(key, name.c_str(), nullptr, &type, data.data(), &got);
    RegCloseKey(key);

    if (r != ERROR_SUCCESS)
        return false;

    data.resize(got);
    return true;
}

RegVal decodeRegValue(DWORD type, const vector<BYTE>& data)
{
    RegVal v;
    v.found = true;
    v.type = type;

    if (type == REG_DWORD && data.size() >= sizeof(DWORD))
    {
        memcpy(&v.num, data.data(), sizeof(DWORD));
    }
    else if (type == REG_SZ || type == REG_EXPAND_SZ)
    {
        size_t chars = data.size() / sizeof(wchar_t);
        const wchar_t* s = (const wchar_t*)data.data();
        size_t len = 0;

        while (len < chars && s[len])
            ++len;

        v.str = wstring(s, len);
    }
    else if (type == REG_MULTI_SZ)
    {
        size_t chars = data.size() / sizeof(wchar_t);
        const wchar_t* s = (const wchar_t*)data.data();
        size_t pos = 0;

        while (pos < chars && s[pos])
        {
            size_t len = 0;

            while (pos + len < chars && s[pos + len])
                ++len;

            if (!v.str.empty())
                v.str += L" | ";

            v.str += wstring(s + pos, len);
            pos += len + 1;
        }
    }

    return v;
}

RegVal regRead(HKEY root, const wchar_t* path, const wchar_t* name,
    REGSAM view = KEY_WOW64_64KEY)
{
    DWORD type = 0;
    vector<BYTE> data;

    if (!regReadRaw(root, path ? path : L"", name ? name : L"", view, type, data))
        return RegVal();

    return decodeRegValue(type, data);
}

wstring regText(const RegVal& v)
{
    if (!v.found)
        return L"(отсутствует)";

    if (v.type == REG_DWORD)
        return to_wstring(v.num);

    if (v.type == REG_BINARY)
        return L"(двоичные данные)";

    if (v.str.empty())
        return L"(пусто)";

    return v.str;
}

struct AllowRule
{
    const wchar_t* prefix;
    const wchar_t* values;
    int            mode;
};

bool serviceFixAllowed(const wstring& lowPath, const wstring& lowName)
{
    const wstring base = L"system\\currentcontrolset\\services\\";

    if (lowPath.compare(0, base.size(), base) != 0)
        return false;

    wstring rest = lowPath.substr(base.size());

    if (lowName == L"start")
    {
        static const wchar_t* names[] =
        { L"wuauserv", L"bits", L"mpssvc", L"eventlog", L"winmgmt" };

        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
            if (rest == names[i])
                return true;
    }

    const wstring fw = L"sharedaccess\\parameters\\firewallpolicy";

    if (lowName == L"enablefirewall" && rest.compare(0, fw.size(), fw) == 0)
        return true;

    return false;
}

bool regChangeAllowed(HKEY root, const wstring& path, const wstring& name)
{
    wstring low = lowerW(path);

    for (;;)
    {
        size_t at = low.find(L"wow6432node\\");

        if (at == wstring::npos)
            break;

        low.erase(at, 12);
    }

    wstring lowName = lowerW(name);
    const wstring winnt = L"software\\microsoft\\windows nt\\currentversion\\";
    const wstring cv = L"software\\microsoft\\windows\\currentversion\\";

    if (serviceFixAllowed(low, lowName))
        return true;

    struct Rule { wstring prefix; wstring values; int mode; };

    static const vector<Rule> rules =
    {
        { L"software\\myscannerrecovery", L"", 1 },
        { winnt + L"winlogon", L"", 1 },
        { winnt + L"windows", L"", 0 },
        { winnt + L"image file execution options", L"debugger", 1 },
        { winnt + L"silentprocessexit", L"monitorprocess", 1 },
        { winnt + L"aedebug", L"debugger", 1 },
        { winnt + L"fonts", L"", 0 },
        { L"system\\currentcontrolset\\control\\session manager\\appcertdlls", L"", 0 },
        { L"system\\currentcontrolset\\control\\session manager",
          L"bootexecute|setupexecute|platformexecute", 0 },
        { cv + L"policies", L"", 1 },
        { L"software\\policies", L"", 1 },
        { cv + L"run", L"", 2 },
        { cv + L"explorer\\startupapproved", L"", 1 },
        { cv + L"explorer\\advanced", L"hidden|hidefileext|showsuperhidden|checkedvalue", 1 },
        { cv + L"internet settings", L"proxyenable|proxyserver|autoconfigurl|autodetect", 0 },
        { L"software\\microsoft\\security center", L"", 1 },
        { L"software\\microsoft\\windows script host\\settings", L"enabled", 0 },
        { L"software\\classes\\.exe", L"<default>", 0 },
        { L"software\\classes\\.bat", L"<default>", 0 },
        { L"software\\classes\\.cmd", L"<default>", 0 },
        { L"software\\classes\\.com", L"<default>", 0 },
        { L"software\\classes\\.lnk", L"<default>", 0 },
        { L"software\\classes\\.reg", L"<default>", 0 },
        { L"software\\classes\\exefile\\shell\\open\\command", L"<default>", 0 }
    };

    for (size_t i = 0; i < rules.size(); ++i)
    {
        const Rule& r = rules[i];
        bool pathOk = false;

        if (r.mode == 0)
            pathOk = low == r.prefix;
        else if (r.mode == 1)
            pathOk = low == r.prefix || low.compare(0, r.prefix.size() + 1, r.prefix + L"\\") == 0;
        else
            pathOk = low.compare(0, r.prefix.size(), r.prefix) == 0;

        if (!pathOk)
            continue;

        if (r.values.empty())
            return true;

        wstring token;
        wstring list = r.values + L"|";

        for (size_t k = 0; k < list.size(); ++k)
        {
            if (list[k] == L'|')
            {
                if (token == L"<default>" ? lowName.empty() : token == lowName)
                    return true;

                token.clear();
            }
            else
            {
                token.push_back(list[k]);
            }
        }
    }

    (void)root;
    return false;
}

const wchar_t* JOURNAL_PATH = L"Software\\MyScannerRecovery\\Changes";
const DWORD JOURNAL_ABSENT = 0xFFFFFFFF;
const size_t JOURNAL_LIMIT = 300;

struct JournalEntry
{
    wstring id;
    wstring root;
    wstring path;
    wstring name;
    wstring time;
    REGSAM  view = 0;
    bool    offline = false;
    DWORD   oldType = JOURNAL_ABSENT;
    vector<BYTE> oldData;
};

void kvSetStr(HKEY key, const wchar_t* name, const wstring& value)
{
    (void)RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)value.c_str(),
        (DWORD)((value.size() + 1) * sizeof(wchar_t)));
}

void kvSetDword(HKEY key, const wchar_t* name, DWORD value)
{
    (void)RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
}

wstring kvGetStr(HKEY key, const wchar_t* name)
{
    DWORD type = 0;
    DWORD size = 0;

    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ)
        return wstring();

    vector<BYTE> buf((size_t)size + 4, 0);
    DWORD got = size;

    if (RegQueryValueExW(key, name, nullptr, &type, buf.data(), &got) != ERROR_SUCCESS)
        return wstring();

    buf[got] = 0;
    buf[got + 1] = 0;
    return wstring((const wchar_t*)buf.data());
}

DWORD kvGetDword(HKEY key, const wchar_t* name, DWORD fallback)
{
    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);

    if (RegQueryValueExW(key, name, nullptr, &type, (BYTE*)&value, &size) != ERROR_SUCCESS ||
        type != REG_DWORD)
        return fallback;

    return value;
}

vector<wstring> journalIds()
{
    vector<wstring> ids;
    HKEY key = nullptr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, JOURNAL_PATH, 0,
        KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return ids;

    for (DWORD i = 0; ; ++i)
    {
        wchar_t name[128] = {};
        DWORD len = 128;

        LONG r = RegEnumKeyExW(key, i, name, &len, nullptr, nullptr, nullptr, nullptr);

        if (r == ERROR_NO_MORE_ITEMS)
            break;

        if (r == ERROR_SUCCESS)
            ids.push_back(name);
    }

    RegCloseKey(key);
    sort(ids.begin(), ids.end());
    return ids;
}

void journalDeleteId(const wstring& id)
{
    (void)RegDeleteKeyExW(HKEY_CURRENT_USER,
        (wstring(JOURNAL_PATH) + L"\\" + id).c_str(), KEY_WOW64_64KEY, 0);
}

void journalPush(HKEY root, const wstring& path, const wstring& name, REGSAM view)
{
    if (isOwnKey(path) || path.find(L"StartupApproved") != wstring::npos)
        return;

    DWORD type = 0;
    vector<BYTE> data;
    bool had = regReadRaw(root, path, name, view, type, data);

    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);

    static unsigned counter = 0;

    ULONGLONG t = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

    wchar_t id[64] = {};
    swprintf_s(id, L"%016llX%02u", t, (counter++) % 100);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t stamp[64] = {};
    swprintf_s(stamp, L"%02d.%02d.%04d %02d:%02d:%02d", (int)st.wDay, (int)st.wMonth,
        (int)st.wYear, (int)st.wHour, (int)st.wMinute, (int)st.wSecond);

    HKEY key = nullptr;
    wstring sub = wstring(JOURNAL_PATH) + L"\\" + id;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, sub.c_str(), 0, nullptr, 0,
        KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;

    kvSetStr(key, L"Root", rootName(root));
    kvSetStr(key, L"Path", path);
    kvSetStr(key, L"Name", name);
    kvSetStr(key, L"Time", stamp);
    kvSetDword(key, L"View", (DWORD)view);
    kvSetDword(key, L"Offline", g_off.active ? 1u : 0u);
    kvSetDword(key, L"OldType", had ? type : JOURNAL_ABSENT);

    (void)RegSetValueExW(key, L"OldData", 0, REG_BINARY,
        data.empty() ? (const BYTE*)"" : data.data(), (DWORD)data.size());

    RegCloseKey(key);

    vector<wstring> ids = journalIds();

    for (size_t i = 0; ids.size() > JOURNAL_LIMIT && i < ids.size() - JOURNAL_LIMIT; ++i)
        journalDeleteId(ids[i]);
}

vector<JournalEntry> journalList()
{
    vector<JournalEntry> list;
    vector<wstring> ids = journalIds();

    for (size_t n = ids.size(); n > 0; --n)
    {
        HKEY key = nullptr;
        wstring sub = wstring(JOURNAL_PATH) + L"\\" + ids[n - 1];

        if (RegOpenKeyExW(HKEY_CURRENT_USER, sub.c_str(), 0,
            KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
            continue;

        JournalEntry e;
        e.id = ids[n - 1];
        e.root = kvGetStr(key, L"Root");
        e.path = kvGetStr(key, L"Path");
        e.name = kvGetStr(key, L"Name");
        e.time = kvGetStr(key, L"Time");
        e.view = (REGSAM)kvGetDword(key, L"View", KEY_WOW64_64KEY);
        e.offline = kvGetDword(key, L"Offline", 0) != 0;
        e.oldType = kvGetDword(key, L"OldType", JOURNAL_ABSENT);

        DWORD type = 0;
        DWORD size = 0;

        if (RegQueryValueExW(key, L"OldData", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
            size > 0)
        {
            e.oldData.assign(size, 0);

            if (RegQueryValueExW(key, L"OldData", nullptr, &type, e.oldData.data(),
                &size) != ERROR_SUCCESS)
                e.oldData.clear();
        }

        RegCloseKey(key);
        list.push_back(e);
    }

    return list;
}

bool regSetRaw(HKEY root, const wstring& path, const wstring& name, REGSAM view,
    DWORD type, const BYTE* data, DWORD size)
{
    HKEY key = nullptr;

    if (regOpenKey(root, path, KEY_SET_VALUE, view, &key, true) != ERROR_SUCCESS)
        return false;

    LONG r = RegSetValueExW(key, name.c_str(), 0, type,
        data ? data : (const BYTE*)"", size);

    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool regDeleteRaw(HKEY root, const wstring& path, const wstring& name, REGSAM view)
{
    HKEY key = nullptr;

    LONG r = regOpenKey(root, path, KEY_SET_VALUE, view, &key);

    if (r == ERROR_FILE_NOT_FOUND)
        return true;

    if (r != ERROR_SUCCESS)
        return false;

    r = RegDeleteValueW(key, name.c_str());
    RegCloseKey(key);

    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

bool regDelete(HKEY root, const wstring& path, const wstring& name,
    REGSAM view = KEY_WOW64_64KEY)
{
    if (!regChangeAllowed(root, path, name))
    {
        logw(L"Заблокировано удаление вне белого списка: " + rootName(root) + L"\\" + path + L"\\" + name);
        return false;
    }

    DWORD type = 0;
    vector<BYTE> data;

    if (!regReadRaw(root, path, name, view, type, data))
        return true;

    journalPush(root, path, name, view);
    return regDeleteRaw(root, path, name, view);
}

bool regWriteTyped(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, DWORD type, const BYTE* data, DWORD size)
{
    if (!regChangeAllowed(root, path, name))
    {
        logw(L"Заблокирована запись вне белого списка: " + rootName(root) + L"\\" + path + L"\\" + name);
        return false;
    }

    journalPush(root, path, name, view);
    return regSetRaw(root, path, name, view, type, data, size);
}

bool regWriteStr(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, const wstring& value, DWORD type = REG_SZ)
{
    return regWriteTyped(root, path, name, view, type,
        (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
}

bool regWriteDword(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, DWORD value)
{
    return regWriteTyped(root, path, name, view, REG_DWORD,
        (const BYTE*)&value, sizeof(value));
}

bool regWriteBinary(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, const BYTE* data, DWORD size)
{
    return regWriteTyped(root, path, name, view, REG_BINARY, data, size);
}

bool regWriteMulti(HKEY root, const wstring& path, const wstring& name,
    REGSAM view, const vector<wstring>& parts)
{
    wstring buf;

    for (size_t i = 0; i < parts.size(); ++i)
    {
        buf += parts[i];
        buf.push_back(L'\0');
    }

    buf.push_back(L'\0');

    if (parts.empty())
        buf.push_back(L'\0');

    return regWriteTyped(root, path, name, view, REG_MULTI_SZ,
        (const BYTE*)buf.data(), (DWORD)(buf.size() * sizeof(wchar_t)));
}

wstring journalDescribe(const JournalEntry& e)
{
    wstring oldText = L"(значения не было)";

    if (e.oldType != JOURNAL_ABSENT)
        oldText = regText(decodeRegValue(e.oldType, e.oldData));

    wstring leaf = e.name.empty() ? wstring(L"(по умолчанию)") : e.name;
    return e.time + L"  " + e.root + L"\\..\\" + leaf + L"  было: " + oldText;
}

bool journalUndo(const JournalEntry& e)
{
    if (e.offline != g_off.active)
        return false;

    HKEY root = e.root == L"HKLM" ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    if (!regChangeAllowed(root, e.path, e.name))
        return false;

    bool ok;

    if (e.oldType == JOURNAL_ABSENT)
        ok = regDeleteRaw(root, e.path, e.name, e.view);
    else
        ok = regSetRaw(root, e.path, e.name, e.view, e.oldType,
            e.oldData.data(), (DWORD)e.oldData.size());

    if (ok)
    {
        journalDeleteId(e.id);
        logw(L"Откат изменения: " + e.root + L"\\" + e.path + L"\\" + e.name);
    }

    return ok;
}

wstring liveWinDir()
{
    wchar_t win[MAX_PATH] = {};
    UINT n = GetSystemWindowsDirectoryW(win, MAX_PATH);
    return wstring(win, n);
}

wstring targetWinDir()
{
    return g_off.active ? g_off.winDir : liveWinDir();
}

wstring targetDrive()
{
    wstring w = targetWinDir();
    return w.size() >= 2 ? w.substr(0, 2) : wstring(L"C:");
}

wstring targetSys32()
{
    return targetWinDir() + L"\\System32";
}

bool enablePrivilege(const wchar_t* name)
{
    HANDLE token = nullptr;

    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool ok = LookupPrivilegeValueW(nullptr, name, &tp.Privileges[0].Luid) &&
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr) &&
        GetLastError() == ERROR_SUCCESS;

    CloseHandle(token);
    return ok;
}

bool fileExists(const wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool dirExists(const wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

vector<wstring> findWindowsInstalls()
{
    vector<wstring> found;
    wstring live = lowerW(liveWinDir());
    DWORD drives = GetLogicalDrives();

    for (int i = 0; i < 26; ++i)
    {
        if (!(drives & (1u << i)))
            continue;

        wstring root = wstring(1, (wchar_t)(L'A' + i)) + L":\\";
        UINT type = GetDriveTypeW(root.c_str());

        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
            continue;

        wstring win = root + L"Windows";

        if (lowerW(win) == live)
            continue;

        if (fileExists(win + L"\\System32\\config\\SYSTEM") &&
            fileExists(win + L"\\System32\\config\\SOFTWARE"))
            found.push_back(win);
    }

    return found;
}

vector<wstring> findUserProfiles(const wstring& winDir)
{
    vector<wstring> users;
    wstring dir = winDir.substr(0, 2) + L"\\Users";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);

    if (h == INVALID_HANDLE_VALUE)
        return users;

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        wstring n = fd.cFileName;
        wstring low = lowerW(n);

        if (low == L"." || low == L".." || low == L"public" || low == L"default" ||
            low == L"default user" || low == L"all users")
            continue;

        if (fileExists(dir + L"\\" + n + L"\\NTUSER.DAT"))
            users.push_back(n);
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return users;
}

void unmountOffline()
{
    if (g_off.userLoaded)
    {
        for (int i = 0; i < 5 && RegUnLoadKeyW(HKEY_LOCAL_MACHINE, OFF_USER) != ERROR_SUCCESS; ++i)
            Sleep(200);
    }

    if (g_off.systemLoaded)
    {
        for (int i = 0; i < 5 && RegUnLoadKeyW(HKEY_LOCAL_MACHINE, OFF_SYSTEM) != ERROR_SUCCESS; ++i)
            Sleep(200);
    }

    if (g_off.softwareLoaded)
    {
        for (int i = 0; i < 5 && RegUnLoadKeyW(HKEY_LOCAL_MACHINE, OFF_SOFTWARE) != ERROR_SUCCESS; ++i)
            Sleep(200);
    }

    if (g_off.active)
        logw(L"Офлайн-режим отключён: " + g_off.winDir);

    g_off = OfflineState();
}

bool mountOffline(const wstring& winDir, const wstring& userName, wstring& error)
{
    unmountOffline();

    (void)enablePrivilege(SE_BACKUP_NAME);
    (void)enablePrivilege(SE_RESTORE_NAME);

    wstring cfg = winDir + L"\\System32\\config\\";

    LONG r = RegLoadKeyW(HKEY_LOCAL_MACHINE, OFF_SOFTWARE, (cfg + L"SOFTWARE").c_str());

    if (r != ERROR_SUCCESS)
    {
        error = L"Не удалось загрузить куст SOFTWARE. Код: " + to_wstring(r);
        return false;
    }

    g_off.softwareLoaded = true;

    r = RegLoadKeyW(HKEY_LOCAL_MACHINE, OFF_SYSTEM, (cfg + L"SYSTEM").c_str());

    if (r != ERROR_SUCCESS)
    {
        error = L"Не удалось загрузить куст SYSTEM. Код: " + to_wstring(r);
        unmountOffline();
        return false;
    }

    g_off.systemLoaded = true;

    HKEY sel = nullptr;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (wstring(OFF_SYSTEM) + L"\\Select").c_str(), 0,
        KEY_QUERY_VALUE, &sel) == ERROR_SUCCESS)
    {
        DWORD cur = kvGetDword(sel, L"Current", 1);
        wchar_t cs[32] = {};
        swprintf_s(cs, L"ControlSet%03u", (unsigned)cur);
        g_off.controlSet = cs;
        RegCloseKey(sel);
    }

    if (!userName.empty())
    {
        wstring hive = winDir.substr(0, 2) + L"\\Users\\" + userName + L"\\NTUSER.DAT";

        if (RegLoadKeyW(HKEY_LOCAL_MACHINE, OFF_USER, hive.c_str()) == ERROR_SUCCESS)
        {
            g_off.userLoaded = true;
            g_off.userName = userName;
        }
    }

    g_off.winDir = winDir;
    g_off.active = true;

    logw(L"Офлайн-режим: подключена система " + winDir +
        (g_off.userLoaded ? L", профиль " + userName : wstring()));

    return true;
}

struct EnvInfo
{
    bool winpe = false;
    int  cleanBoot = 0;
};

EnvInfo g_env;

bool regKeyExists(HKEY root, const wchar_t* path)
{
    HKEY key = nullptr;

    if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return false;

    RegCloseKey(key);
    return true;
}

void detectEnvironment()
{
    g_env.cleanBoot = GetSystemMetrics(SM_CLEANBOOT);
    g_env.winpe = regKeyExists(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\MiniNT");

    if (!g_env.winpe)
    {
        wchar_t drive[16] = {};

        if (GetEnvironmentVariableW(L"SystemDrive", drive, 16) &&
            _wcsicmp(drive, L"X:") == 0)
            g_env.winpe = true;
    }
}

wstring envName()
{
    if (g_env.winpe)
        return L"WinRE / WinPE";

    if (g_env.cleanBoot == 1)
        return L"Безопасный режим";

    if (g_env.cleanBoot == 2)
        return L"Безопасный режим (сеть)";

    return L"Windows";
}

// ============================================================
// НАСТРОЙКИ (сохраняются между запусками)
// ============================================================

const wchar_t* SETTINGS_PATH = L"Software\\MyScannerRecovery\\Settings";

void saveSettings()
{
    regWriteDword(HKEY_CURRENT_USER, SETTINGS_PATH, L"Theme",
        KEY_WOW64_64KEY, (DWORD)currentTheme);
    regWriteDword(HKEY_CURRENT_USER, SETTINGS_PATH, L"Emoji",
        KEY_WOW64_64KEY, useEmoji ? 1u : 0u);
}

void loadSettings()
{
    RegVal t = regRead(HKEY_CURRENT_USER, SETTINGS_PATH, L"Theme");
    RegVal e = regRead(HKEY_CURRENT_USER, SETTINGS_PATH, L"Emoji");

    if (t.found && t.type == REG_DWORD && t.num < themes.size())
        currentTheme = (int)t.num;

    if (e.found && e.type == REG_DWORD)
        useEmoji = e.num != 0;
}

// ============================================================
// РАМКИ И ОФОРМЛЕНИЕ
// ============================================================

const int BOX_W = 66;

typedef vector<pair<wstring, WORD> > Segs;

void boxTop()
{
    color(THEME_MAIN);
    p(L"  \u2554" + wstring(BOX_W, L'\u2550') + L"\u2557\n");
}

void boxMid()
{
    color(THEME_MAIN);
    p(L"  \u2560" + wstring(BOX_W, L'\u2550') + L"\u2563\n");
}

void boxBottom()
{
    color(THEME_MAIN);
    p(L"  \u255A" + wstring(BOX_W, L'\u2550') + L"\u255D\n");
}

void boxSeg(const Segs& segs, bool center)
{
    size_t len = 0;

    for (size_t i = 0; i < segs.size(); ++i)
        len += segs[i].first.size();

    size_t total = (size_t)BOX_W;

    if (len > total)
        len = total;

    size_t left = center ? (total - len) / 2 : 1;
    size_t right = total - len - left;

    color(THEME_MAIN);
    p(L"  \u2551" + wstring(left, L' '));

    for (size_t i = 0; i < segs.size(); ++i)
    {
        color(segs[i].second);
        p(segs[i].first);
    }

    color(THEME_MAIN);
    p(wstring(right, L' ') + L"\u2551\n");
}

void boxLine(const wstring& text, WORD c, bool center = false)
{
    wstring t = center ? trimW(text) : text;
    t = clip(t, (size_t)BOX_W - 2);

    Segs s;
    s.push_back(make_pair(t, c));
    boxSeg(s, center);
}

void divider()
{
    color(THEME_MAIN);
    p(L"\n  " + wstring(BOX_W + 2, L'\u2550') + L"\n");
}

enum Health { H_OK = 0, H_WARN = 1, H_BAD = 2 };

void healthIcon(Health h)
{
    if (useEmoji)
    {
        color(h == H_OK ? THEME_MAIN : (h == H_WARN ? C_YELLOW : THEME_ERROR));
        p(h == H_OK ? L"\u2705 " : (h == H_WARN ? L"\u26A0\uFE0F " : L"\u274C "));
    }
    else
    {
        color(h == H_OK ? THEME_MAIN : (h == H_WARN ? C_YELLOW : THEME_ERROR));
        p(h == H_OK ? L"[OK] " : (h == H_WARN ? L"[!!] " : L"[XX] "));
    }
}

void status(const wstring& text, bool ok)
{
    color(THEME_TEXT);
    p(L"  [ ");

    color(ok ? THEME_MAIN : THEME_ERROR);
    p(ok ? L"OK" : L"ОШИБКА");

    color(THEME_TEXT);
    p(L" ] " + text + L"\n");
}

wstring g_screen;

void header(const wstring& title)
{
    if (title != g_screen)
    {
        clear();
        g_screen = title;
    }
    else
    {
        home();
    }

    showCursor(false);

    p(L"\n");
    boxTop();
    boxLine(L"M Y S C A N   R E C O V E R Y", THEME_MAIN, true);
    boxBottom();

    color(THEME_TEXT);
    p(L"\n  МОДУЛЬ :: ");

    color(THEME_MAIN);
    p(title);

    divider();
}

void pauseKey()
{
    color(THEME_DIM);
    p(L"\n  [ НАЖМИТЕ ЛЮБУЮ КЛАВИШУ ]");
    readKey();
}

bool confirm(const wstring& message)
{
    eraseBelow();
    color(THEME_TEXT);
    p(L"\n  " + message + L"\n");

    color(THEME_MAIN);
    p(L"  Подтвердить? [Y / Д — да, любая другая клавиша — отмена] : ");

    int k = (int)_getwch();

    if (k == 0 || k == 0xE0)
    {
        _getwch();
        k = 0;
    }

    p(L"\n");

    return k == 'y' || k == 'Y' || k == 0x434 || k == 0x414;
}

bool requireAdmin()
{
    if (admin())
        return true;

    status(L"Требуются права администратора.", false);
    pauseKey();
    return false;
}

wstring readLine(const wstring& prompt)
{
    eraseBelow();
    color(THEME_MAIN);
    p(prompt);
    color(THEME_TEXT);

    showCursor(true);

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD oldMode = 0;
    (void)GetConsoleMode(in, &oldMode);

    (void)SetConsoleMode(in, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
        ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);

    wchar_t buf[512] = {};
    DWORD n = 0;

    BOOL ok = ReadConsoleW(in, buf, 511, &n, nullptr);

    (void)FlushConsoleInputBuffer(in);
    (void)SetConsoleMode(in, oldMode);
    showCursor(false);

    if (!ok)
        return wstring();

    return trimW(wstring(buf, n));
}

bool editLine(const wstring& prompt, wstring& text, size_t maxLen = 480)
{
    eraseBelow();
    color(THEME_MAIN);
    p(prompt);

    COORD c = cursorPos();
    int baseX = (int)c.X - g_margin;
    int baseY = (int)c.Y;
    int viewW = CON_COLS - baseX - 3;

    if (viewW < 16)
        viewW = 16;

    size_t pos = text.size();
    size_t off = 0;
    bool accepted = false;

    showCursor(true);

    for (;;)
    {
        if (pos < off)
            off = pos;

        if (pos > off + (size_t)viewW - 1)
            off = pos - (size_t)viewW + 1;

        wstring vis = sanitize(text.substr(off, (size_t)viewW));

        if (vis.size() < (size_t)viewW)
            vis.append((size_t)viewW - vis.size(), L' ');

        showCursor(false);
        gotoxy(baseX, baseY);
        color(THEME_TEXT);
        rawWrite(vis.c_str(), vis.size());
        gotoxy(baseX + (int)(pos - off), baseY);
        showCursor(true);

        int k = readKeyCore();

        if (k == K_ENTER)
        {
            accepted = true;
            break;
        }

        if (k == K_ESC)
            break;

        if (k == K_LEFT && pos > 0)
            --pos;
        else if (k == K_RIGHT && pos < text.size())
            ++pos;
        else if (k == K_HOME)
            pos = 0;
        else if (k == K_END)
            pos = text.size();
        else if (k == 8 && pos > 0)
            text.erase(--pos, 1);
        else if (k == K_DEL && pos < text.size())
            text.erase(pos, 1);
        else if (k >= 32 && k < 0xFFFF && text.size() < maxLen)
            text.insert(pos++, 1, (wchar_t)k);
    }

    showCursor(false);
    p(L"\n");
    return accepted;
}

// ============================================================
// ЗАПУСК СИСТЕМНЫХ УТИЛИТ: вывод в реальном времени,
// прогресс, отмена по ESC, корректная работа с UTF-16 (sfc)
// ============================================================

enum RunResult { RUN_OK, RUN_FAILED, RUN_CANCELLED, RUN_NOSTART };

bool parsePercent(const wstring& line, int& out)
{
    size_t pct = line.rfind(L'%');

    if (pct == wstring::npos)
        return false;

    size_t b = pct;

    while (b > 0 && (iswdigit(line[b - 1]) ||
        line[b - 1] == L'.' || line[b - 1] == L','))
        --b;

    if (b >= pct)
        return false;

    wstring num = line.substr(b, pct - b);

    for (size_t i = 0; i < num.size(); ++i)
        if (num[i] == L',')
            num[i] = L'.';

    double v = wcstod(num.c_str(), nullptr);

    if (v < 0) v = 0;
    if (v > 100) v = 100;

    out = (int)v;
    return true;
}

struct TextStream
{
    string          carry;
    wstring         line;
    vector<wstring> lines;
    int             enc = 0;
    bool            vt = false;
    int             vstate = 0;

    void breakLine()
    {
        if (!line.empty())
        {
            lines.push_back(line);
            line.clear();
        }
    }

    void emit(wchar_t ch)
    {
        if (ch == L'\r' || ch == L'\n')
            breakLine();
        else if (ch == 8)
        {
            if (!line.empty())
                line.pop_back();
        }
        else if (ch == L'\t')
            line.push_back(L' ');
        else if (ch >= 32 && ch != 0xFEFF)
            line.push_back(ch);

        if (line.size() > 4096)
            breakLine();
    }

    void csiFinal(wchar_t f)
    {
        switch (f)
        {
        case L'H': case L'f': case L'A': case L'B':
        case L'E': case L'F': case L'd':
            breakLine();
            break;
        case L'C':
            if (!line.empty() && line.back() != L' ')
                line.push_back(L' ');
            break;
        }
    }

    void process(const wstring& w)
    {
        for (size_t i = 0; i < w.size(); ++i)
        {
            wchar_t ch = w[i];

            if (!vt)
            {
                emit(ch);
                continue;
            }

            switch (vstate)
            {
            case 0:
                if (ch == 0x1B)
                    vstate = 1;
                else
                    emit(ch);
                break;
            case 1:
                if (ch == L'[')
                    vstate = 2;
                else if (ch == L']')
                    vstate = 3;
                else if (ch == L'(' || ch == L')')
                    vstate = 5;
                else
                    vstate = 0;
                break;
            case 2:
                if (ch >= 0x40 && ch <= 0x7E)
                {
                    csiFinal(ch);
                    vstate = 0;
                }
                break;
            case 3:
                if (ch == 7)
                    vstate = 0;
                else if (ch == 0x1B)
                    vstate = 4;
                break;
            default:
                vstate = 0;
                break;
            }
        }
    }

    void feed(const char* data, size_t n)
    {
        carry.append(data, n);

        if (enc == 0)
        {
            if (carry.size() < 2)
                return;

            bool zeros = false;
            size_t lim = min(carry.size(), (size_t)64);

            for (size_t i = 1; i < lim; i += 2)
            {
                if (carry[i] == 0 && carry[i - 1] != 0)
                {
                    zeros = true;
                    break;
                }
            }

            enc = zeros ? 2 : 1;
        }

        wstring wide;

        if (enc == 2)
        {
            size_t usable = carry.size() & ~(size_t)1;

            for (size_t i = 0; i < usable; i += 2)
                wide.push_back((wchar_t)((unsigned char)carry[i] |
                    ((unsigned char)carry[i + 1] << 8)));

            carry.erase(0, usable);
        }
        else if (enc == 3)
        {
            size_t cut = carry.size();

            for (size_t back = 1; back <= 3 && back <= carry.size(); ++back)
            {
                unsigned char c = (unsigned char)carry[carry.size() - back];

                if ((c & 0xC0) == 0x80)
                    continue;

                size_t need = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : (c >= 0xC0 ? 2 : 1));

                if (need > back)
                    cut = carry.size() - back;

                break;
            }

            wide = fromUtf8(carry.substr(0, cut));
            carry.erase(0, cut);
        }
        else
        {
            int count = MultiByteToWideChar(CP_OEMCP, 0, carry.data(),
                (int)carry.size(), nullptr, 0);

            if (count > 0)
            {
                wide.assign((size_t)count, L'\0');
                MultiByteToWideChar(CP_OEMCP, 0, carry.data(),
                    (int)carry.size(), &wide[0], count);
            }

            carry.clear();
        }

        process(wide);
    }
};

typedef HRESULT(WINAPI* CreatePseudoConsoleFn)(COORD, HANDLE, HANDLE, DWORD, HANDLE*);
typedef void (WINAPI* ClosePseudoConsoleFn)(HANDLE);

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

struct Proc
{
    HANDLE process = nullptr;
    HANDLE thread = nullptr;
    HANDLE out = nullptr;
    HANDLE in = nullptr;
    HANDLE job = nullptr;
    HANDLE pty = nullptr;
    bool   inJob = false;
    bool   usesPty = false;
    DWORD  error = 0;
};

HANDLE makeKillJob()
{
    HANDLE job = CreateJobObjectW(nullptr, nullptr);

    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
            &info, sizeof(info)))
        {
            CloseHandle(job);
            job = nullptr;
        }
    }

    return job;
}

bool launchPty(const wstring& exe, vector<wchar_t>& cmd, Proc& pr)
{
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");

    if (!k32)
        return false;

    CreatePseudoConsoleFn create =
        (CreatePseudoConsoleFn)GetProcAddress(k32, "CreatePseudoConsole");
    ClosePseudoConsoleFn closeFn =
        (ClosePseudoConsoleFn)GetProcAddress(k32, "ClosePseudoConsole");

    if (!create || !closeFn)
        return false;

    HANDLE inRd = nullptr, inWr = nullptr, outRd = nullptr, outWr = nullptr;

    if (!CreatePipe(&inRd, &inWr, nullptr, 0))
        return false;

    if (!CreatePipe(&outRd, &outWr, nullptr, 1 << 20))
    {
        CloseHandle(inRd);
        CloseHandle(inWr);
        return false;
    }

    COORD size = { 120, 40 };
    HANDLE hpc = nullptr;

    if (FAILED(create(size, inRd, outWr, 0, &hpc)))
    {
        CloseHandle(inRd);
        CloseHandle(inWr);
        CloseHandle(outRd);
        CloseHandle(outWr);
        return false;
    }

    CloseHandle(inRd);
    CloseHandle(outWr);

    STARTUPINFOEXW six{};
    six.StartupInfo.cb = sizeof(six);

    SIZE_T attrSize = 0;
    (void)InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);

    vector<BYTE> attr(attrSize);
    six.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attr.data();

    bool started = false;
    PROCESS_INFORMATION pi{};

    if (InitializeProcThreadAttributeList(six.lpAttributeList, 1, 0, &attrSize))
    {
        if (UpdateProcThreadAttribute(six.lpAttributeList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), nullptr, nullptr))
        {
            started = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr,
                &six.StartupInfo, &pi) != FALSE;
        }

        (void)DeleteProcThreadAttributeList(six.lpAttributeList);
    }

    if (!started)
    {
        pr.error = GetLastError();
        closeFn(hpc);
        CloseHandle(inWr);
        CloseHandle(outRd);
        return false;
    }

    pr.process = pi.hProcess;
    pr.thread = pi.hThread;
    pr.in = inWr;
    pr.out = outRd;
    pr.pty = hpc;
    pr.usesPty = true;
    return true;
}

bool launchProcess(const wstring& exe, const wstring& args, bool wantPty, Proc& pr)
{
    wstring cmdLine = L"\"" + exe + L"\"";

    if (!args.empty())
        cmdLine += L" " + args;

    vector<wchar_t> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back(L'\0');

    bool ok = false;

    if (wantPty)
        ok = launchPty(exe, cmd, pr);

    if (!ok)
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE rd = nullptr;
        HANDLE wr = nullptr;

        if (!CreatePipe(&rd, &wr, &sa, 0))
        {
            pr.error = GetLastError();
            return false;
        }

        (void)SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

        HANDLE nul = CreateFileW(L"NUL", GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = wr;
        si.hStdError = wr;
        si.hStdInput = (nul != INVALID_HANDLE_VALUE) ? nul : nullptr;

        PROCESS_INFORMATION pi{};

        BOOL created = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr,
            TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);

        pr.error = GetLastError();

        CloseHandle(wr);

        if (nul != INVALID_HANDLE_VALUE)
            CloseHandle(nul);

        if (!created)
        {
            CloseHandle(rd);
            return false;
        }

        pr.process = pi.hProcess;
        pr.thread = pi.hThread;
        pr.out = rd;
    }

    pr.job = makeKillJob();
    pr.inJob = pr.job && AssignProcessToJobObject(pr.job, pr.process);

    (void)ResumeThread(pr.thread);
    return true;
}

void killProc(Proc& pr)
{
    if (pr.inJob)
        (void)TerminateJobObject(pr.job, 1);
    else
        (void)TerminateProcess(pr.process, 1);
}

void closeProc(Proc& pr)
{
    if (pr.pty)
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        ClosePseudoConsoleFn closeFn = k32
            ? (ClosePseudoConsoleFn)GetProcAddress(k32, "ClosePseudoConsole") : nullptr;

        if (closeFn)
            closeFn(pr.pty);

        pr.pty = nullptr;
    }

    if (pr.in) CloseHandle(pr.in);
    if (pr.out) CloseHandle(pr.out);
    if (pr.thread) CloseHandle(pr.thread);
    if (pr.process) CloseHandle(pr.process);
    if (pr.job) CloseHandle(pr.job);

    pr = Proc();
}

bool pumpPipe(Proc& pr, TextStream& ts)
{
    DWORD avail = 0;

    if (!PeekNamedPipe(pr.out, nullptr, 0, nullptr, &avail, nullptr))
        return false;

    while (avail)
    {
        char chunk[4096];
        DWORD got = 0;
        DWORD want = min(avail, (DWORD)sizeof(chunk));

        if (!ReadFile(pr.out, chunk, want, &got, nullptr) || !got)
            return false;

        ts.feed(chunk, got);
        avail -= min(avail, got);
    }

    return true;
}

bool confirmKey(const wchar_t* prompt)
{
    color(C_YELLOW);
    p(prompt);

    int a = (int)_getwch();

    if (a == 0 || a == 0xE0)
    {
        (void)_getwch();
        a = 0;
    }

    p(L"\n");

    return a == 'y' || a == 'Y' || a == 0x434 || a == 0x414;
}

RunResult runProcess(const wstring& exe, const wstring& args,
    const wstring& label, bool parseProgress = false, bool usePty = false)
{
    if (!fileExists(exe))
    {
        status(L"Файл не найден: " + exe, false);
        return RUN_NOSTART;
    }

    logw(L"Запуск: " + label);

    Proc pr;

    if (!launchProcess(exe, args, usePty, pr))
    {
        status(L"Не удалось запустить процесс. Код: " + to_wstring(pr.error), false);
        return RUN_NOSTART;
    }

    eraseBelow();
    color(THEME_ACCENT);
    p(L"\n  // ВЫВОД СИСТЕМНОЙ УТИЛИТЫ  (ESC — отмена) //\n\n");
    color(THEME_TEXT);

    TextStream ts;
    ts.vt = pr.usesPty;
    ts.enc = pr.usesPty ? 3 : 0;

    int progress = -1;
    bool statusVisible = false;
    bool cancelled = false;
    unsigned tick = 0;
    vector<wstring> recent;

    ULONGLONG start = GetTickCount64();

    const int BAR_W = 30;

    auto clearStatus = [&]()
        {
            if (statusVisible)
            {
                p(L"\r" + wstring(90, L' ') + L"\r");
                statusVisible = false;
            }
        };

    auto drawStatus = [&]()
        {
            ULONGLONG secs = (GetTickCount64() - start) / 1000;

            wchar_t clock[32] = {};
            swprintf_s(clock, L"%02u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));

            p(L"\r  ");

            if (progress >= 0)
            {
                int filled = progress * BAR_W / 100;

                color(THEME_MAIN);
                p(wstring((size_t)filled, L'█'));
                color(THEME_DIM);
                p(wstring((size_t)(BAR_W - filled), L'░'));
            }
            else
            {
                int span = BAR_W - 6;
                int pos = (int)((tick / 2) % (unsigned)(span * 2));

                if (pos >= span)
                    pos = span * 2 - pos - 1;

                color(THEME_DIM);
                p(wstring((size_t)pos, L'░'));
                color(THEME_MAIN);
                p(wstring(6, L'█'));
                color(THEME_DIM);
                p(wstring((size_t)(BAR_W - 6 - pos), L'░'));
            }

            color(THEME_TEXT);

            wstring tail = (progress >= 0
                ? L" " + to_wstring(progress) + L"%"
                : wstring(L" ..."));

            tail += L"  " + wstring(clock) + L"   ESC — отмена";

            p(padR(tail, 50));

            statusVisible = true;
        };

    auto handleLine = [&](wstring line)
        {
            line = trimW(line);

            if (line.empty())
                return;

            int pct = 0;

            if (parseProgress && parsePercent(line, pct))
            {
                progress = pct;
                return;
            }

            if (ts.vt)
            {
                if (find(recent.begin(), recent.end(), line) != recent.end())
                    return;

                recent.push_back(line);

                if (recent.size() > 8)
                    recent.erase(recent.begin());
            }

            clearStatus();
            color(THEME_TEXT);
            p(L"  " + line + L"\n");
        };

    auto consume = [&]()
        {
            vector<wstring> done;
            done.swap(ts.lines);

            for (size_t i = 0; i < done.size(); ++i)
                handleLine(done[i]);

            int pct = 0;

            if (parseProgress && !ts.line.empty() && parsePercent(ts.line, pct))
                progress = pct;
        };

    for (;;)
    {
        if (!pumpPipe(pr, ts))
        {
            consume();
            break;
        }

        consume();

        if (WaitForSingleObject(pr.process, 0) == WAIT_OBJECT_0)
        {
            int idle = 0;

            while (idle < 3)
            {
                DWORD avail = 0;

                if (!PeekNamedPipe(pr.out, nullptr, 0, nullptr, &avail, nullptr))
                    break;

                if (avail)
                {
                    if (!pumpPipe(pr, ts))
                        break;

                    idle = 0;
                }
                else
                {
                    Sleep(40);
                    ++idle;
                }
            }

            consume();
            break;
        }

        if (_kbhit())
        {
            int k = readKeyRaw();

            if (k == K_ESC)
            {
                clearStatus();

                if (confirmKey(L"\n  Прервать операцию? [Y — да / любая другая — продолжить] : "))
                {
                    killProc(pr);
                    cancelled = true;
                    break;
                }

                color(THEME_TEXT);
            }
        }

        if (GetTickCount64() - start > 400)
            drawStatus();

        ++tick;
        Sleep(50);
    }

    clearStatus();

    if (!ts.line.empty())
    {
        wstring rest = ts.line;
        ts.line.clear();

        int pct = 0;

        if (!(parseProgress && parsePercent(rest, pct)))
            handleLine(rest);
    }

    DWORD waited = WaitForSingleObject(pr.process, cancelled ? 5000 : 3000);

    if (waited == WAIT_TIMEOUT && !cancelled)
        killProc(pr);

    DWORD exitCode = 1;
    (void)GetExitCodeProcess(pr.process, &exitCode);

    closeProc(pr);

    divider();

    if (cancelled)
    {
        color(C_YELLOW);
        p(L"  Операция отменена пользователем.\n");
        logw(L"Отменена операция: " + label);
        return RUN_CANCELLED;
    }

    if (parseProgress && exitCode == 0)
    {
        color(THEME_MAIN);
        p(L"  " + wstring((size_t)BAR_W, L'█') + L" 100%\n");
    }

    status(L"Процесс завершён. Код: " + to_wstring(exitCode), exitCode == 0);

    logw(L"Завершена операция: " + label + L". Код: " + to_wstring(exitCode));

    return exitCode == 0 ? RUN_OK : RUN_FAILED;
}

bool captureProcess(const wstring& exe, const wstring& args, wstring& out,
    DWORD timeoutMs = 20000, DWORD* exitCode = nullptr)
{
    out.clear();

    if (exitCode)
        *exitCode = 1;

    if (!fileExists(exe))
        return false;

    Proc pr;

    if (!launchProcess(exe, args, false, pr))
        return false;

    TextStream ts;
    ULONGLONG start = GetTickCount64();
    bool timedOut = false;

    for (;;)
    {
        if (!pumpPipe(pr, ts))
            break;

        if (WaitForSingleObject(pr.process, 0) == WAIT_OBJECT_0)
        {
            (void)pumpPipe(pr, ts);
            break;
        }

        if (GetTickCount64() - start > timeoutMs)
        {
            killProc(pr);
            timedOut = true;
            break;
        }

        Sleep(20);
    }

    ts.breakLine();

    for (size_t i = 0; i < ts.lines.size(); ++i)
        out += ts.lines[i] + L"\n";

    DWORD code = 1;
    (void)WaitForSingleObject(pr.process, 2000);
    (void)GetExitCodeProcess(pr.process, &code);

    if (exitCode)
        *exitCode = code;

    closeProc(pr);
    return !timedOut;
}

vector<wstring> splitLines(const wstring& text)
{
    vector<wstring> lines;
    wstring cur;

    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\n')
        {
            lines.push_back(cur);
            cur.clear();
        }
        else if (text[i] != L'\r')
        {
            cur.push_back(text[i]);
        }
    }

    if (!cur.empty())
        lines.push_back(cur);

    return lines;
}

// ============================================================
// СВЕДЕНИЯ ОБ УСТРОЙСТВАХ (реальная проверка исправности)
// ============================================================

struct DevInfo
{
    wstring name;
    ULONG problem;
};

const GUID CLS_DISPLAY = { 0x4d36e968, 0xe325, 0x11ce,
    { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
const GUID CLS_DISK = { 0x4d36e967, 0xe325, 0x11ce,
    { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
const GUID CLS_NET = { 0x4d36e972, 0xe325, 0x11ce,
    { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
const GUID CLS_MEDIA = { 0x4d36e96c, 0xe325, 0x11ce,
    { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
const GUID CLS_CPU = { 0x50127dc3, 0x0f36, 0x415e,
    { 0xa6, 0xcc, 0x4c, 0xb3, 0xbe, 0x91, 0x0b, 0x65 } };

vector<DevInfo> enumDevicesImpl(const GUID* cls)
{
    vector<DevInfo> out;

    HDEVINFO h = SetupDiGetClassDevsW(cls, nullptr, nullptr,
        cls ? DIGCF_PRESENT : (DIGCF_PRESENT | DIGCF_ALLCLASSES));

    if (h == INVALID_HANDLE_VALUE)
        return out;

    SP_DEVINFO_DATA d{};
    d.cbSize = sizeof(d);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &d); ++i)
    {
        ULONG st = 0;
        ULONG problem = 0;

        if (CM_Get_DevNode_Status(&st, &problem, d.DevInst, 0) != CR_SUCCESS)
        {
            st = 0;
            problem = 0;
        }

        wchar_t name[256] = {};
        DWORD req = 0;

        if (!SetupDiGetDeviceRegistryPropertyW(h, &d, SPDRP_FRIENDLYNAME,
            nullptr, (PBYTE)name, sizeof(name) - sizeof(wchar_t), &req))
        {
            (void)SetupDiGetDeviceRegistryPropertyW(h, &d, SPDRP_DEVICEDESC,
                nullptr, (PBYTE)name, sizeof(name) - sizeof(wchar_t), &req);
        }

        DevInfo di;
        di.name = name[0] ? wstring(name) : wstring(L"Неизвестное устройство");
        di.problem = (st & DN_HAS_PROBLEM) ? problem : 0;

        out.push_back(di);
    }

    SetupDiDestroyDeviceInfoList(h);
    return out;
}

vector<DevInfo> enumDevices(const GUID& cls)
{
    return enumDevicesImpl(&cls);
}

vector<DevInfo> enumAllDevices()
{
    return enumDevicesImpl(nullptr);
}

Health devHealthOne(const DevInfo& d)
{
    if (d.problem == 0)
        return H_OK;

    if (d.problem == CM_PROB_DISABLED)
        return H_WARN;

    return H_BAD;
}

Health devHealthAll(const vector<DevInfo>& v)
{
    Health worst = H_OK;

    for (size_t i = 0; i < v.size(); ++i)
    {
        Health h = devHealthOne(v[i]);

        if ((int)h > (int)worst)
            worst = h;
    }

    return worst;
}

wstring devNote(const DevInfo& d)
{
    if (d.problem == 0)
        return L"";

    if (d.problem == CM_PROB_DISABLED)
        return L"  (отключено)";

    return L"  (ошибка устройства, код " + to_wstring(d.problem) + L")";
}

// ============================================================
// ПРОВЕРКА КОМПЛЕКТУЮЩИХ
// ============================================================

int chooseMenu(const wstring& title, const wstring& intro,
    const vector<wstring>& names, const vector<wstring>& descs, int& sel)
{
    int count = (int)names.size();

    for (;;)
    {
        header(title);

        color(THEME_TEXT);
        p(L"\n  " + intro + L"\n\n");

        for (int i = 0; i < count; ++i)
        {
            wstring num = (i + 1 < 10 ? L" " : L"") + to_wstring(i + 1);

            if (i == sel)
            {
                color(THEME_MAIN);
                p(L"  ► " + num + L". " + names[i] + L"\n");
                color(THEME_TEXT);
                p(L"         " + clip(descs[i], 84) + L"\n");
            }
            else
            {
                color(THEME_DIM);
                p(L"    " + num + L". " + names[i] + L"\n");
            }
        }

        color(THEME_DIM);
        p(L"\n  ↑↓ — выбор | ENTER — открыть | ESC — назад\n");

        int k = readKey();

        if (k == K_ESC || k == '0')
            return -1;

        if (k == K_UP)
            sel = (sel + count - 1) % count;
        else if (k == K_DOWN)
            sel = (sel + 1) % count;
        else if (k >= '1' && k <= '9' && k - '1' < count)
        {
            sel = k - '1';
            return sel;
        }
        else if (k == K_ENTER)
            return sel;
    }
}

volatile LONG g_testStop = 0;

bool escPressed()
{
    while (_kbhit())
    {
        if (readKeyRaw() == K_ESC)
            return true;
    }

    return false;
}

void progressLine(int pct, const wstring& tail)
{
    const int W = 30;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    int filled = pct * W / 100;

    p(L"\r  ");
    color(THEME_MAIN);
    p(wstring((size_t)filled, L'█'));
    color(THEME_DIM);
    p(wstring((size_t)(W - filled), L'░'));
    color(THEME_TEXT);

    wchar_t num[16] = {};
    swprintf_s(num, L" %3d%%  ", pct);

    p(padR(wstring(num) + tail, 56));
}

void testVerdict(bool ok, const wstring& okText, const wstring& badText)
{
    p(L"\n\n  ");
    healthIcon(ok ? H_OK : H_BAD);
    color(ok ? THEME_MAIN : THEME_ERROR);
    p((ok ? okText : badText) + L"\n");
}

void runMemoryTest()
{
    header(L"ТЕСТ ОПЕРАТИВНОЙ ПАМЯТИ");

    color(THEME_TEXT);
    p(L"\n  Тест выделяет часть свободной памяти, записывает в неё шаблоны\n");
    p(L"  и проверяет их. Проверяется только память, доступная программе.\n");
    p(L"  Полную проверку выполняет диагностика памяти Windows.\n\n");

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);

    if (!GlobalMemoryStatusEx(&ms))
    {
        status(L"Не удалось получить данные о памяти.", false);
        pauseKey();
        return;
    }

    ULONGLONG want = ms.ullAvailPhys / 4;

    if (want > (1ULL << 30))
        want = 1ULL << 30;

    if (want < (32ULL << 20))
        want = 32ULL << 20;

    SIZE_T size = (SIZE_T)(want & ~(ULONGLONG)4095);

    void* mem = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!mem)
    {
        status(L"Не удалось выделить память для теста.", false);
        pauseKey();
        return;
    }

    p(L"  Проверяется " + fmtGB(size, 2) + L" ГБ. ESC — отмена.\n\n");

    volatile ULONGLONG* a = (volatile ULONGLONG*)mem;
    const size_t n = size / sizeof(ULONGLONG);
    const size_t chunk = 1u << 20;
    const int passes = 6;

    ULONGLONG errors = 0;
    bool cancelled = false;
    ULONGLONG started = GetTickCount64();

    for (int pass = 0; pass < passes && !cancelled; ++pass)
    {
        ULONGLONG fixed[4] = { 0ULL, ~0ULL, 0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL };

        for (int stage = 0; stage < 2 && !cancelled; ++stage)
        {
            ULONGLONG seed = 0x9E3779B97F4A7C15ULL ^ (ULONGLONG)pass;
            ULONGLONG x = seed;

            for (size_t i = 0; i < n; ++i)
            {
                ULONGLONG expected;

                if (pass < 4)
                {
                    expected = fixed[pass];
                }
                else if (pass == 4)
                {
                    expected = (ULONGLONG)i * 0x0101010101010101ULL;
                }
                else
                {
                    x ^= x << 13;
                    x ^= x >> 7;
                    x ^= x << 17;
                    expected = x;
                }

                if (stage == 0)
                {
                    a[i] = expected;
                }
                else if (a[i] != expected)
                {
                    ++errors;
                }

                if ((i & (chunk - 1)) == 0)
                {
                    int total = passes * 2;
                    int step = pass * 2 + stage;
                    int pct = (int)(((ULONGLONG)step * n + i) * 100 / ((ULONGLONG)total * n));

                    progressLine(pct, stage == 0 ? L"запись шаблона " + to_wstring(pass + 1) + L"/" + to_wstring(passes)
                        : L"проверка шаблона " + to_wstring(pass + 1) + L"/" + to_wstring(passes));

                    if (escPressed())
                    {
                        cancelled = true;
                        break;
                    }
                }
            }
        }
    }

    (void)VirtualFree(mem, 0, MEM_RELEASE);

    if (cancelled)
    {
        p(L"\n\n  ");
        color(C_YELLOW);
        p(L"Тест отменён пользователем.\n");
        logw(L"Тест оперативной памяти отменён.");
    }
    else
    {
        progressLine(100, L"готово");

        ULONGLONG ms2 = GetTickCount64() - started;

        testVerdict(errors == 0,
            L"Ошибок не обнаружено (" + to_wstring(ms2 / 1000) + L" с).",
            L"Обнаружено ошибок: " + to_wstring(errors) + L". Проверьте модули памяти.");

        logw(L"Тест оперативной памяти: ошибок " + to_wstring(errors));
    }

    pauseKey();
}

struct CpuWork
{
    ULONGLONG endTick;
    ULONGLONG refX;
    double    refD;
    ULONGLONG rounds;
    ULONGLONG errors;
};

ULONGLONG cpuKernel(double& out)
{
    ULONGLONG x = 88172645463325252ULL;
    double d = 1.0;

    for (int i = 0; i < 20000; ++i)
    {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        x *= 0x9E3779B97F4A7C15ULL;

        d = d * 1.0000001 + (double)(x & 0xFF) * 1e-9;

        if (d > 2.0)
            d -= 1.0;
    }

    out = d;
    return x;
}

DWORD WINAPI cpuThread(LPVOID param)
{
    CpuWork* w = (CpuWork*)param;

    while (GetTickCount64() < w->endTick && !g_testStop)
    {
        double d = 0;
        ULONGLONG x = cpuKernel(d);

        if (x != w->refX || fabs(d - w->refD) > 1e-9)
            ++w->errors;

        ++w->rounds;
    }

    return 0;
}

void runCpuTest()
{
    header(L"НАГРУЗОЧНЫЙ ТЕСТ ПРОЦЕССОРА");

    color(THEME_TEXT);
    p(L"\n  Все потоки процессора выполняют одинаковые вычисления,\n");
    p(L"  результаты сверяются с эталоном. ESC — отмена.\n\n");

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    DWORD threads = si.dwNumberOfProcessors;

    if (threads < 1) threads = 1;
    if (threads > 64) threads = 64;

    double refD = 0;
    ULONGLONG refX = cpuKernel(refD);

    const DWORD durationMs = 8000;
    ULONGLONG started = GetTickCount64();

    vector<CpuWork> work(threads);
    vector<HANDLE> handles;

    g_testStop = 0;

    for (DWORD i = 0; i < threads; ++i)
    {
        work[i].endTick = started + durationMs;
        work[i].refX = refX;
        work[i].refD = refD;
        work[i].rounds = 0;
        work[i].errors = 0;

        HANDLE h = CreateThread(nullptr, 0, cpuThread, &work[i], 0, nullptr);

        if (h)
            handles.push_back(h);
    }

    if (handles.empty())
    {
        status(L"Не удалось запустить потоки теста.", false);
        pauseKey();
        return;
    }

    bool cancelled = false;

    while (WaitForMultipleObjects((DWORD)handles.size(), handles.data(), TRUE, 100) == WAIT_TIMEOUT)
    {
        ULONGLONG elapsed = GetTickCount64() - started;
        progressLine((int)(elapsed * 100 / durationMs), L"потоков: " + to_wstring(threads));

        if (escPressed())
        {
            g_testStop = 1;
            cancelled = true;
        }
    }

    ULONGLONG rounds = 0;
    ULONGLONG errors = 0;

    for (size_t i = 0; i < handles.size(); ++i)
        CloseHandle(handles[i]);

    for (DWORD i = 0; i < threads; ++i)
    {
        rounds += work[i].rounds;
        errors += work[i].errors;
    }

    g_testStop = 0;

    if (cancelled)
    {
        p(L"\n\n  ");
        color(C_YELLOW);
        p(L"Тест отменён пользователем.\n");
    }
    else
    {
        progressLine(100, L"готово");

        p(L"\n\n  Выполнено вычислительных циклов: " + to_wstring(rounds) + L"\n");

        testVerdict(errors == 0,
            L"Ошибок вычислений не обнаружено.",
            L"Обнаружено ошибок вычислений: " + to_wstring(errors) + L". Возможен перегрев или нестабильное питание.");
    }

    logw(L"Тест процессора: циклов " + to_wstring(rounds) + L", ошибок " + to_wstring(errors));
    pauseKey();
}

wstring powerShellExe()
{
    return sysDir() + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
}

RunResult runPowerShell(const wstring& command, const wstring& label)
{
    if (!fileExists(powerShellExe()))
    {
        status(L"PowerShell недоступен в этой среде.", false);
        return RUN_NOSTART;
    }

    return runProcess(powerShellExe(),
        L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + command + L"\"",
        label);
}

void runDiskHealth()
{
    header(L"СОСТОЯНИЕ НАКОПИТЕЛЕЙ");

    color(THEME_TEXT);
    p(L"\n  Состояние дисков и прогноз отказа по данным SMART.\n");

    runPowerShell(
        L"Get-PhysicalDisk | Select-Object FriendlyName,MediaType,HealthStatus,OperationalStatus,"
        L"@{n='GB';e={[math]::Round($_.Size/1GB)}} | Format-Table -AutoSize | Out-String -Width 110; "
        L"Get-CimInstance -Namespace root\\wmi -ClassName MSStorageDriver_FailurePredictStatus "
        L"-ErrorAction SilentlyContinue | Select-Object InstanceName,PredictFailure | "
        L"Format-Table -AutoSize | Out-String -Width 110",
        L"Состояние накопителей");

    pauseKey();
}

bool pickDriveLetter(wstring& drive)
{
    wstring input = readLine(L"\n  Буква тома (ENTER — системный " + targetDrive() + L"): ");

    if (input.empty())
    {
        drive = targetDrive();
        return true;
    }

    if (input.size() > 2 || !iswalpha(input[0]) || input[0] > 127 ||
        (input.size() == 2 && input[1] != L':'))
    {
        status(L"Нужна одна буква диска, например C.", false);
        return false;
    }

    wchar_t letter = (wchar_t)towupper(input[0]);
    drive = wstring(1, letter) + L":";

    UINT type = GetDriveTypeW((drive + L"\\").c_str());

    if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
    {
        status(L"Такой том недоступен для проверки.", false);
        return false;
    }

    return true;
}

void runChkdsk()
{
    header(L"ПРОВЕРКА ФАЙЛОВОЙ СИСТЕМЫ");

    color(THEME_TEXT);
    p(L"\n  chkdsk запускается только в режиме чтения: ошибки\n");
    p(L"  показываются, но не исправляются.\n");

    wstring drive;

    if (pickDriveLetter(drive))
        runProcess(sysExe(L"chkdsk.exe"), drive, L"CHKDSK " + drive);

    pauseKey();
}

void showBattery()
{
    header(L"БАТАРЕЯ И ПИТАНИЕ");

    SYSTEM_POWER_STATUS ps{};

    p(L"\n");

    if (!GetSystemPowerStatus(&ps))
    {
        status(L"Не удалось получить сведения о питании.", false);
        pauseKey();
        return;
    }

    auto row = [&](Health h, const wstring& label, const wstring& value)
        {
            p(L"    ");
            healthIcon(h);
            color(THEME_ACCENT);
            p(padR(label, 16));
            color(THEME_TEXT);
            p(value + L"\n");
        };

    row(H_OK, L"Питание",
        ps.ACLineStatus == 1 ? L"от сети" : (ps.ACLineStatus == 0 ? L"от батареи" : L"неизвестно"));

    if (ps.BatteryFlag == 128)
    {
        row(H_OK, L"Батарея", L"не обнаружена (стационарный компьютер)");
    }
    else
    {
        int level = ps.BatteryLifePercent == 255 ? -1 : (int)ps.BatteryLifePercent;

        Health h = level < 0 ? H_WARN : (level < 10 ? H_BAD : (level < 20 ? H_WARN : H_OK));

        row(h, L"Заряд", level < 0 ? wstring(L"неизвестно") : to_wstring(level) + L"%");

        row((ps.BatteryFlag & 8) ? H_OK : H_OK, L"Зарядка",
            (ps.BatteryFlag & 8) ? L"идёт" : L"не идёт");

        if (ps.BatteryLifeTime != 0xFFFFFFFF)
            row(H_OK, L"Осталось", to_wstring(ps.BatteryLifeTime / 3600) + L" ч " +
                to_wstring((ps.BatteryLifeTime % 3600) / 60) + L" мин");
    }

    p(L"\n");
    color(THEME_DIM);
    p(L"  Износ батареи: powercfg /batteryreport (создаёт файл отчёта).\n");

    pauseKey();
}

void runNetworkTest()
{
    header(L"ПРОВЕРКА СЕТИ");

    color(THEME_TEXT);
    p(L"\n  Проверка доступности адреса и разрешения имён.\n");

    RunResult r = runProcess(sysExe(L"ping.exe"), L"-n 4 1.1.1.1", L"PING 1.1.1.1");

    if (r != RUN_CANCELLED)
        runProcess(sysExe(L"ping.exe"), L"-n 2 www.microsoft.com", L"PING www.microsoft.com");

    pauseKey();
}

wstring problemText(ULONG code)
{
    switch (code)
    {
    case 1:  return L"устройство настроено неправильно";
    case 10: return L"устройство не запускается";
    case 12: return L"нет свободных ресурсов";
    case 14: return L"требуется перезагрузка";
    case 18: return L"нужно переустановить драйверы";
    case 19: return L"повреждён реестр конфигурации";
    case 22: return L"устройство отключено";
    case 24: return L"устройство отсутствует или не работает";
    case 28: return L"драйверы не установлены";
    case 31: return L"устройство работает неправильно";
    case 39: return L"драйвер повреждён или отсутствует";
    case 43: return L"устройство остановлено из-за ошибок";
    case 52: return L"нет цифровой подписи драйвера";
    default: return L"код ошибки " + to_wstring(code);
    }
}

void showDeviceErrors()
{
    header(L"УСТРОЙСТВА С ОШИБКАМИ");

    vector<DevInfo> all = enumAllDevices();
    int bad = 0;

    p(L"\n");

    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].problem == 0)
            continue;

        ++bad;

        if (bad > 28)
            continue;

        p(L"  ");
        healthIcon(devHealthOne(all[i]));
        color(THEME_TEXT);
        p(clip(all[i].name, 44) + L"  ");
        color(THEME_DIM);
        p(clip(problemText(all[i].problem), 40) + L"\n");
    }

    if (bad == 0)
    {
        p(L"  ");
        healthIcon(H_OK);
        color(THEME_MAIN);
        p(L"Проверено устройств: " + to_wstring(all.size()) + L". Ошибок драйверов нет.\n");
    }
    else
    {
        color(THEME_ERROR);
        p(L"\n  Устройств с ошибками: " + to_wstring(bad) + L" из " + to_wstring(all.size()) + L"\n");
    }

    pauseKey();
}

void runMemoryDiag()
{
    header(L"ДИАГНОСТИКА ПАМЯТИ WINDOWS");

    color(THEME_TEXT);
    p(L"\n  Полная проверка выполняется при загрузке компьютера.\n");

    if (g_env.winpe)
    {
        status(L"Недоступно в WinRE.", false);
        pauseKey();
        return;
    }

    if (confirm(L"Запустить диагностику памяти Windows? Компьютер будет перезагружен."))
    {
        (void)ShellExecuteW(nullptr, L"open", sysExe(L"mdsched.exe").c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);

        logw(L"Запущена диагностика оперативной памяти.");
    }

    pauseKey();
}

void hardwareMenu()
{
    vector<wstring> names =
    {
        L"Оперативная память (тест в программе)",
        L"Процессор (нагрузочный тест)",
        L"Накопители (состояние и SMART)",
        L"Файловая система (chkdsk, чтение)",
        L"Батарея и питание",
        L"Сеть (ping и DNS)",
        L"Устройства с ошибками драйверов",
        L"Диагностика памяти Windows (перезагрузка)"
    };

    vector<wstring> descs =
    {
        L"Запись и проверка шаблонов в выделенной памяти, ESC — отмена",
        L"Расчёты на всех потоках со сверкой результата, около 8 секунд",
        L"Состояние дисков и прогноз отказа через PowerShell",
        L"Проверка тома без исправлений",
        L"Тип питания, заряд и состояние батареи",
        L"Доступность интернета и работа DNS",
        L"Все классы устройств с кодами проблем",
        L"Полная проверка памяти при следующей загрузке"
    };

    int sel = 0;

    for (;;)
    {
        int pick = chooseMenu(L"ПРОВЕРКА КОМПЛЕКТУЮЩИХ",
            L"Выберите проверку:", names, descs, sel);

        switch (pick)
        {
        case 0: runMemoryTest();   break;
        case 1: runCpuTest();      break;
        case 2: runDiskHealth();   break;
        case 3: runChkdsk();       break;
        case 4: showBattery();     break;
        case 5: runNetworkTest();  break;
        case 6: showDeviceErrors(); break;
        case 7: runMemoryDiag();   break;
        default: return;
        }
    }
}

// ============================================================
// ИНФОРМАЦИЯ О СИСТЕМЕ
// ============================================================

void systemInfo()
{
    header(L"ИНФОРМАЦИЯ О СИСТЕМЕ");

    int problems = 0;

    auto section = [&](const wstring& title)
        {
            color(THEME_ACCENT);
            p(L"\n  \u25A0 " + title + L"\n");
        };

    auto row = [&](Health h, const wstring& label, const wstring& value)
        {
            if (h != H_OK)
                ++problems;

            p(L"    ");
            healthIcon(h);
            color(THEME_ACCENT);
            p(padR(label, 13));
            color(THEME_TEXT);
            p(clip(value, 70) + L"\n");
        };

    section(L"КОМПЬЮТЕР");

    wchar_t host[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD hostSize = MAX_COMPUTERNAME_LENGTH + 1;

    if (!GetComputerNameW(host, &hostSize))
        host[0] = 0;

    row(H_OK, L"Имя / учётка", wstring(host) + L"  /  " + currentUser);

    section(L"ПРОЦЕССОР");

    RegVal cpu = regRead(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);

    {
        vector<DevInfo> cpus = enumDevices(CLS_CPU);
        Health h = cpu.found ? devHealthAll(cpus) : H_WARN;

        row(h, L"Процессор",
            (cpu.found ? trimW(cpu.str) : wstring(L"Не определён")) +
            L"  [потоков: " + to_wstring(sysInfo.dwNumberOfProcessors) + L"]");
    }

    section(L"ВИДЕОКАРТА");

    {
        vector<DevInfo> gpus = enumDevices(CLS_DISPLAY);

        if (gpus.empty())
            row(H_WARN, L"Видеокарта", L"Не обнаружена");

        for (size_t i = 0; i < gpus.size(); ++i)
            row(devHealthOne(gpus[i]), L"Видеокарта",
                gpus[i].name + devNote(gpus[i]));
    }

    section(L"ОПЕРАТИВНАЯ ПАМЯТЬ");

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);

    if (GlobalMemoryStatusEx(&mem))
    {
        Health h = mem.dwMemoryLoad < 85 ? H_OK :
            (mem.dwMemoryLoad < 95 ? H_WARN : H_BAD);

        row(h, L"Память",
            L"Всего " + fmtGB(mem.ullTotalPhys, 2) + L" ГБ | Свободно " +
            fmtGB(mem.ullAvailPhys, 2) + L" ГБ | Загрузка " +
            to_wstring(mem.dwMemoryLoad) + L"%");
    }
    else
    {
        row(H_WARN, L"Память", L"Не удалось получить данные");
    }

    section(L"НАКОПИТЕЛИ");

    {
        vector<DevInfo> disks = enumDevices(CLS_DISK);

        for (size_t i = 0; i < disks.size(); ++i)
            row(devHealthOne(disks[i]), L"Накопитель",
                disks[i].name + devNote(disks[i]));

        DWORD drives = GetLogicalDrives();

        for (int i = 0; i < 26; ++i)
        {
            if (!(drives & (1u << i)))
                continue;

            wchar_t root[] = L"A:\\";
            root[0] = (wchar_t)(L'A' + i);

            UINT type = GetDriveTypeW(root);

            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
                continue;

            ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};

            if (!GetDiskFreeSpaceExW(root, &freeBytes, &totalBytes, &totalFree)
                || totalBytes.QuadPart == 0)
                continue;

            ULONGLONG pct = freeBytes.QuadPart * 100 / totalBytes.QuadPart;

            Health h = pct < 5 ? H_BAD : (pct < 10 ? H_WARN : H_OK);

            wstring label = wstring(1, root[0]) + L":";

            row(h, L"Диск " + label,
                L"Всего " + fmtGB(totalBytes.QuadPart, 1) + L" ГБ | Свободно " +
                fmtGB(freeBytes.QuadPart, 1) + L" ГБ (" +
                to_wstring(pct) + L"%)");
        }
    }

    section(L"СЕТЬ И ЗВУК");

    {
        vector<DevInfo> nets = enumDevices(CLS_NET);
        size_t shown = 0;

        for (size_t i = 0; i < nets.size() && shown < 4; ++i)
        {
            row(devHealthOne(nets[i]), L"Сеть",
                nets[i].name + devNote(nets[i]));
            ++shown;
        }

        if (nets.size() > shown)
            row(H_OK, L"Сеть", L"и ещё адаптеров: " +
                to_wstring(nets.size() - shown));

        vector<DevInfo> snd = enumDevices(CLS_MEDIA);

        for (size_t i = 0; i < snd.size() && i < 2; ++i)
            row(devHealthOne(snd[i]), L"Звук",
                snd[i].name + devNote(snd[i]));
    }

    section(L"WINDOWS");

    {
        const wchar_t* cv = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

        RegVal prod = regRead(HKEY_LOCAL_MACHINE, cv, L"ProductName");
        RegVal build = regRead(HKEY_LOCAL_MACHINE, cv, L"CurrentBuild");
        RegVal ubr = regRead(HKEY_LOCAL_MACHINE, cv, L"UBR");
        RegVal disp = regRead(HKEY_LOCAL_MACHINE, cv, L"DisplayVersion");

        wstring name = prod.found ? prod.str : wstring(L"Windows");

        if (build.found && _wtoi(build.str.c_str()) >= 22000)
        {
            size_t at = name.find(L"Windows 10");

            if (at != wstring::npos)
                name.replace(at, 10, L"Windows 11");
        }

        wstring buildText = build.found ? build.str : wstring(L"?");

        if (ubr.found && ubr.type == REG_DWORD)
            buildText += L"." + to_wstring(ubr.num);

        row(prod.found ? H_OK : H_WARN, L"Система",
            name + (disp.found ? L" " + disp.str : wstring()) +
            L"  (сборка " + buildText + L")");

        SYSTEM_INFO nsi{};
        GetNativeSystemInfo(&nsi);

        wstring arch = L"x86 / другая";

        if (nsi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
            arch = L"x64";
        else if (nsi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
            arch = L"ARM64";

        row(H_OK, L"Архитектура", arch);

        row(admin() ? H_OK : H_WARN, L"Права",
            admin() ? L"Администратор" : L"Обычный пользователь");
    }

    logw(L"Открыта информация о системе.");

    divider();

    if (problems == 0)
    {
        healthIcon(H_OK);
        color(THEME_MAIN);
        p(L"Все проверенные компоненты исправны.\n");
    }
    else
    {
        healthIcon(H_WARN);
        color(C_YELLOW);
        p(L"Компонентов, требующих внимания: " + to_wstring(problems) + L"\n");
    }

    color(THEME_DIM);
    p(L"\n  [1] Проверка комплектующих (память, процессор, диски, сеть и другое)\n");
    p(L"  [0] Назад\n");

    color(THEME_MAIN);
    p(L"\n  Выбор: ");

    int k = readKey();

    if (k == '1')
        hardwareMenu();
}

// ============================================================
// SFC
// ============================================================

void sfc()
{
    header(L"ПРОВЕРКА СИСТЕМНЫХ ФАЙЛОВ");

    color(THEME_TEXT);

    p(L"\n  Проверка защищённых файлов Windows.\n");
    p(L"  Запускается настоящая системная утилита SFC, проценты выполнения\n");
    p(L"  показываются в реальном времени. ESC — отмена проверки.\n");

    if (g_off.active)
    {
        color(THEME_ACCENT);
        p(L"\n  Офлайн-режим: проверяется " + g_off.winDir + L"\n");
    }

    if (!requireAdmin())
        return;

    if (confirm(L"Запустить sfc /scannow?"))
    {
        header(L"ПРОВЕРКА СИСТЕМНЫХ ФАЙЛОВ");

        wstring args = L"/scannow";

        if (g_off.active)
            args += L" /offbootdir=" + targetDrive() + L"\\ /offwindir=" + targetWinDir();

        RunResult r = runProcess(sysExe(L"sfc.exe"), args, L"SFC " + args, true, true);

        if (r == RUN_OK || r == RUN_FAILED)
        {
            color(THEME_DIM);
            p(L"\n  Подробный журнал: " + targetWinDir() + L"\\Logs\\CBS\\CBS.log\n");
        }
    }

    pauseKey();
}

// ============================================================
// DISM
// ============================================================

void dism()
{
    vector<wstring> items =
    {
        L"Проверить состояние хранилища",
        L"Выполнить полную проверку",
        L"Восстановить хранилище компонентов"
    };

    int selected = 0;

    while (true)
    {
        header(L"ВОССТАНОВЛЕНИЕ WINDOWS");

        color(THEME_TEXT);
        p(L"\n  Выберите операцию:\n\n");

        for (int i = 0; i < (int)items.size(); ++i)
        {
            color(i == selected ? THEME_MAIN : THEME_DIM);

            p((i == selected ? L"  \u25BA " : L"    ") +
                to_wstring(i + 1) + L". " + items[i] + L"\n\n");
        }

        color(THEME_DIM);
        p(L"  \u2191\u2193 — выбор | ENTER — запуск | ESC — назад\n");
        p(L"  Во время проверки ESC отменяет операцию.");

        int k = readKey();

        if (k == K_ESC)
            return;

        if (k == K_UP)
            selected = (selected + 2) % 3;
        else if (k == K_DOWN)
            selected = (selected + 1) % 3;
        else if (k >= '1' && k <= '3')
            selected = k - '1';
        else if (k == K_ENTER)
        {
            if (!requireAdmin())
                continue;

            const wchar_t* actions[3] =
            {
                L"/Cleanup-Image /CheckHealth",
                L"/Cleanup-Image /ScanHealth",
                L"/Cleanup-Image /RestoreHealth"
            };

            wstring target = g_off.active
                ? L"/Image:" + targetDrive() + L"\\ /ScratchDir:" + targetDrive() + L"\\"
                : wstring(L"/Online");

            wstring dismArgs = target + L" " + actions[selected];

            bool go = true;

            if (selected == 2)
                go = confirm(L"Восстановление изменит хранилище компонентов "
                    L"Windows. Лучше не прерывать его после начала.");

            if (go)
            {
                header(L"ВОССТАНОВЛЕНИЕ WINDOWS");

                runProcess(sysExe(L"dism.exe"), dismArgs, L"DISM " + dismArgs, true);
            }

            pauseKey();
        }
    }
}

// ============================================================
// ЗНАЧЕНИЯ ЗАПУСКА WINDOWS (Userinit, Shell, AppInit ...)
// ============================================================

struct BootItem
{
    wstring label;
    HKEY    root;
    wstring path;
    wstring name;
    REGSAM  view;
    bool    dword;
    bool    multi;
    wstring defStr;
    DWORD   defNum;
    bool    absent;
    wstring hint;
};

vector<wstring> splitMulti(const wstring& text)
{
    vector<wstring> parts;
    size_t start = 0;

    for (size_t i = 0; i <= text.size(); ++i)
    {
        if (i == text.size() || text[i] == L'|')
        {
            wstring part = trimW(text.substr(start, i - start));

            if (!part.empty())
                parts.push_back(part);

            start = i + 1;
        }
    }

    return parts;
}

vector<BootItem> makeBootItems()
{
    wstring userinit = targetWinDir() + L"\\system32\\userinit.exe,";

    const wstring wl = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    const wstring wn = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
    const wstring wn32 = L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
    const wstring sm = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager";

    vector<BootItem> v;
    BootItem it;

    it.view = KEY_WOW64_64KEY;
    it.dword = false;
    it.multi = false;
    it.defNum = 0;
    it.absent = false;

    it.label = L"Userinit (HKLM)"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wl; it.name = L"Userinit"; it.defStr = userinit;
    it.hint = L"Программа входа. Вирусы дописывают свой файл через запятую.";
    v.push_back(it);

    it.label = L"Shell (HKLM)"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wl; it.name = L"Shell"; it.defStr = L"explorer.exe";
    it.hint = L"Оболочка рабочего стола. Должно быть: explorer.exe";
    v.push_back(it);

    it.label = L"Shell (HKCU)"; it.root = HKEY_CURRENT_USER;
    it.path = wl; it.name = L"Shell"; it.defStr = L""; it.absent = true;
    it.hint = L"Пользовательская подмена оболочки. В норме не существует.";
    v.push_back(it);

    it.label = L"Userinit (HKCU)"; it.root = HKEY_CURRENT_USER;
    it.path = wl; it.name = L"Userinit"; it.defStr = L""; it.absent = true;
    it.hint = L"Пользовательская подмена входа. В норме не существует.";
    v.push_back(it);

    it.label = L"Taskman (HKLM)"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wl; it.name = L"Taskman"; it.defStr = L""; it.absent = true;
    it.hint = L"Подмена диспетчера задач. В норме не существует.";
    v.push_back(it);

    it.label = L"BootExecute"; it.root = HKEY_LOCAL_MACHINE;
    it.path = sm; it.name = L"BootExecute"; it.multi = true;
    it.defStr = L"autocheck autochk *"; it.absent = false;
    it.hint = L"Программы до входа в систему. Норма: autocheck autochk *";
    v.push_back(it);

    it.label = L"SetupExecute"; it.root = HKEY_LOCAL_MACHINE;
    it.path = sm; it.name = L"SetupExecute"; it.multi = true;
    it.defStr = L""; it.absent = true;
    it.hint = L"Дополнительные программы ранней загрузки. В норме не существует.";
    v.push_back(it);

    it.label = L"PlatformExecute"; it.root = HKEY_LOCAL_MACHINE;
    it.path = sm; it.name = L"PlatformExecute"; it.multi = true;
    it.defStr = L""; it.absent = true;
    it.hint = L"Ещё один ключ ранней загрузки. В норме не существует.";
    v.push_back(it);

    it.multi = false;
    it.absent = false;

    it.label = L"AppInit_DLLs"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wn; it.name = L"AppInit_DLLs"; it.defStr = L"";
    it.hint = L"DLL, внедряемые во все программы. В норме пусто.";
    v.push_back(it);

    it.label = L"AppInit_DLLs (x86)"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wn32; it.name = L"AppInit_DLLs"; it.defStr = L"";
    it.hint = L"То же для 32-битных программ. В норме пусто.";
    v.push_back(it);

    it.label = L"LoadAppInit_DLLs"; it.root = HKEY_LOCAL_MACHINE;
    it.path = wn; it.name = L"LoadAppInit_DLLs"; it.dword = true;
    it.defStr = L""; it.defNum = 0;
    it.hint = L"Включает загрузку AppInit_DLLs. В норме 0.";
    v.push_back(it);

    return v;
}

wstring normBoot(const wstring& s)
{
    wstring r = lowerW(trimW(s));

    while (!r.empty() && (r.back() == L',' || r.back() == L' '))
        r.pop_back();

    return r;
}

Health bootHealth(const BootItem& it, const RegVal& v)
{
    if (it.dword)
    {
        DWORD cur = (v.found && v.type == REG_DWORD) ? v.num : 0;
        return cur == it.defNum ? H_OK : H_BAD;
    }

    if (it.absent)
        return (v.found && !trimW(v.str).empty()) ? H_BAD : H_OK;

    if (it.defStr.empty())
        return (!v.found || trimW(v.str).empty()) ? H_OK : H_BAD;

    if (!v.found)
        return H_BAD;

    return normBoot(v.str) == normBoot(it.defStr) ? H_OK : H_BAD;
}

wstring bootDefaultText(const BootItem& it)
{
    if (it.dword)
        return to_wstring(it.defNum);

    if (it.absent)
        return L"(значения быть не должно)";

    if (it.defStr.empty())
        return L"(пусто)";

    return it.defStr;
}

bool bootWrite(const BootItem& it, const wstring& text, DWORD num)
{
    if (it.dword)
        return regWriteDword(it.root, it.path, it.name, it.view, num);

    if (it.multi)
        return regWriteMulti(it.root, it.path, it.name, it.view, splitMulti(text));

    return regWriteStr(it.root, it.path, it.name, it.view, text);
}

bool bootReset(const BootItem& it)
{
    RegVal old = regRead(it.root, it.path.c_str(), it.name.c_str(), it.view);

    logw(L"Сброс " + it.label + L": было '" + regText(old) + L"'");

    if (it.absent)
        return regDelete(it.root, it.path, it.name, it.view);

    return bootWrite(it, it.defStr, it.defNum);
}

bool bootValidate(const BootItem& it, const wstring& text, DWORD& num, wstring& warning)
{
    warning.clear();

    if (it.dword)
    {
        if (text.empty())
        {
            warning = L"Нужно число.";
            return false;
        }

        wchar_t* end = nullptr;
        num = (DWORD)wcstoul(text.c_str(), &end, 0);

        if (end == text.c_str() || *end != 0)
        {
            warning = L"Нужно число.";
            return false;
        }

        return true;
    }

    wstring low = lowerW(text);
    wstring name = lowerW(it.name);
    bool required = !it.absent && !it.defStr.empty();

    if (text.empty() && required)
    {
        warning = L"Пустое значение сделает Windows неработоспособной.";
        return false;
    }

    if (text.empty())
        return true;

    if (name == L"userinit" && low.find(L"userinit.exe") == wstring::npos)
        warning = L"значение не содержит userinit.exe, вход в Windows может перестать работать.";
    else if (name == L"shell" && it.root == HKEY_LOCAL_MACHINE && low.find(L"explorer.exe") == wstring::npos)
        warning = L"значение не содержит explorer.exe, рабочий стол может не загрузиться.";
    else if (name == L"bootexecute" && low.find(L"autocheck") == wstring::npos)
        warning = L"в BootExecute обычно только autocheck autochk *.";

    return true;
}

void bootValues()
{
    if (!requireAdmin())
        return;

    int sel = 0;
    wstring message;
    bool messageOk = true;

    while (true)
    {
        vector<BootItem> items = makeBootItems();

        header(L"ЗНАЧЕНИЯ ЗАПУСКА WINDOWS");

        color(THEME_TEXT);
        p(L"\n  Ключи, которые вирусы подменяют чаще всего (Winlogon, AppInit).\n\n");

        int bad = 0;

        for (int i = 0; i < (int)items.size(); ++i)
        {
            const BootItem& it = items[i];
            RegVal v = regRead(it.root, it.path.c_str(), it.name.c_str(), it.view);
            Health h = bootHealth(it, v);

            if (h != H_OK)
                ++bad;

            bool s = (i == sel);

            color(s ? THEME_MAIN : THEME_TEXT);
            p(s ? L"  \u25BA " : L"    ");
            p(padR(it.label, 20) + L" ");

            healthIcon(h);

            color(s ? THEME_TEXT : THEME_DIM);
            p(clip(regText(v), 46) + L"\n");

            if (s)
            {
                color(THEME_ACCENT);
                p(L"        Текущее:     " + clip(regText(v), 66) + L"\n");
                p(L"        Стандартное: " + clip(bootDefaultText(it), 66) + L"\n");
                color(THEME_DIM);
                p(L"        " + clip(it.hint, 84) + L"\n");
            }
        }

        divider();

        if (bad == 0)
        {
            color(THEME_MAIN);
            p(L"  Все значения в норме.\n");
        }
        else
        {
            color(THEME_ERROR);
            p(L"  Изменённых значений: " + to_wstring(bad) + L"\n");
        }

        if (!message.empty())
        {
            color(messageOk ? THEME_MAIN : THEME_ERROR);
            p(L"  " + message + L"\n");
        }

        color(THEME_DIM);
        p(L"\n  \u2191\u2193 — выбор | E — изменить (значение подставится для правки)\n");
        p(L"  R — вернуть заводское | A — вернуть все изменённые | ESC — назад\n");

        message.clear();

        int k = readKey();

        if (k == K_ESC || k == '0')
            return;

        if (k == K_UP)
            sel = (sel + (int)items.size() - 1) % (int)items.size();
        else if (k == K_DOWN)
            sel = (sel + 1) % (int)items.size();
        else if (k == 'E')
        {
            const BootItem it = items[sel];
            RegVal cur = regRead(it.root, it.path.c_str(), it.name.c_str(), it.view);

            wstring original = cur.found
                ? (cur.type == REG_DWORD ? to_wstring(cur.num) : cur.str)
                : wstring();

            wstring text = original;

            color(THEME_DIM);
            p(L"\n  Изменение: " + it.label + L"  (ENTER — сохранить, ESC — отмена)\n");

            if (it.multi)
                p(L"  Несколько строк разделяются знаком |\n");

            if (!editLine(L"  Значение: ", text))
            {
                message = L"Изменение отменено.";
                messageOk = true;
                continue;
            }

            text = trimW(text);

            if (text == trimW(original))
            {
                message = L"Значение не изменилось.";
                messageOk = true;
                continue;
            }

            DWORD num = 0;
            wstring warning;

            if (!bootValidate(it, text, num, warning))
            {
                message = warning;
                messageOk = false;
                continue;
            }

            wstring question = text.empty()
                ? L"Удалить значение " + it.label + L"?"
                : L"Записать значение «" + clip(text, 70) + L"» в " + it.label + L"?";

            if (!warning.empty())
                question += L"\n  ВНИМАНИЕ: " + warning;

            if (confirm(question))
            {
                logw(L"Изменение " + it.label + L": было '" + regText(cur) +
                    L"', стало '" + (text.empty() ? wstring(L"(удалено)") : text) + L"'");

                bool ok = text.empty()
                    ? regDelete(it.root, it.path, it.name, it.view)
                    : bootWrite(it, text, num);

                message = ok ? L"Значение сохранено. Откат: Полезные функции — Откат изменений."
                    : L"Не удалось записать значение.";
                messageOk = ok;
            }
        }
        else if (k == 'R')
        {
            const BootItem& it = items[sel];

            if (confirm(L"Вернуть заводское значение для " + it.label + L"?"))
            {
                bool ok = bootReset(it);
                message = ok ? L"Значение восстановлено." : L"Не удалось восстановить значение.";
                messageOk = ok;
            }
        }
        else if (k == 'A')
        {
            if (bad > 0 && confirm(L"Вернуть заводские значения для всех изменённых пунктов?"))
            {
                int okCount = 0;
                int failCount = 0;

                for (size_t i = 0; i < items.size(); ++i)
                {
                    RegVal v = regRead(items[i].root, items[i].path.c_str(),
                        items[i].name.c_str(), items[i].view);

                    if (bootHealth(items[i], v) == H_OK)
                        continue;

                    if (bootReset(items[i]))
                        ++okCount;
                    else
                        ++failCount;
                }

                message = L"Восстановлено: " + to_wstring(okCount) +
                    L", ошибок: " + to_wstring(failCount);
                messageOk = failCount == 0;
            }
        }
    }
}

// ============================================================
// ДИАГНОСТИКА ЗАГРУЗКИ
// ============================================================

void viewBcd()
{
    header(L"КОНФИГУРАЦИЯ ЗАГРУЗЧИКА (BCD)");

    color(THEME_TEXT);
    p(L"\n  Этот пункт только читает конфигурацию BCD.\n");
    p(L"  Изменений в загрузчик он не вносит.\n");

    if (!requireAdmin())
        return;

    if (confirm(L"Показать конфигурацию загрузчика?"))
        runProcess(sysExe(L"bcdedit.exe"), L"/enum all", L"BCDEDIT /ENUM ALL");

    pauseKey();
}

void bootDiag()
{
    vector<wstring> items =
    {
        L"Значения запуска Windows (Userinit, Shell, AppInit)",
        L"Просмотр конфигурации загрузчика (BCD)"
    };

    int selected = 0;

    while (true)
    {
        header(L"ДИАГНОСТИКА ЗАГРУЗКИ");

        color(THEME_TEXT);
        p(L"\n  Выберите раздел:\n\n");

        for (int i = 0; i < (int)items.size(); ++i)
        {
            color(i == selected ? THEME_MAIN : THEME_DIM);

            p((i == selected ? L"  \u25BA " : L"    ") +
                to_wstring(i + 1) + L". " + items[i] + L"\n\n");
        }

        color(THEME_DIM);
        p(L"  \u2191\u2193 — выбор | ENTER — открыть | ESC — назад");

        int k = readKey();

        if (k == K_ESC || k == '0')
            return;

        if (k == K_UP || k == K_DOWN)
            selected = 1 - selected;
        else if (k == '1')
            bootValues();
        else if (k == '2')
            viewBcd();
        else if (k == K_ENTER)
        {
            if (selected == 0)
                bootValues();
            else
                viewBcd();
        }
    }
}

// ============================================================
// ОГРАНИЧЕНИЯ, ПОЛИТИКИ И ОТЛАДЧИКИ ОТ ВИРУСОВ
// ============================================================

struct Finding
{
    HKEY    root;
    REGSAM  view;
    wstring path;
    wstring name;
    wstring desc;
    wstring valueText;
    int     fixKind;
    DWORD   fixNum;
    wstring fixStr;
};

struct PolicyCheck
{
    const wchar_t* path;
    const wchar_t* name;
    const wchar_t* desc;
    int            mode;
    int            roots;
    int            fixKind;
    DWORD          fixNum;
    const wchar_t* fixStr;
};

bool isBlocked(const RegVal& v, int mode, const wchar_t* expected)
{
    if (mode == 3)
    {
        if (!v.found)
            return true;

        return normBoot(v.str) != normBoot(expected ? expected : L"");
    }

    if (!v.found)
        return false;

    if (mode == 2)
        return (v.type == REG_SZ || v.type == REG_EXPAND_SZ) &&
        !trimW(v.str).empty();

    if (mode == 4)
        return v.type == REG_DWORD && v.num == 4;

    DWORD n = 0;

    if (v.type == REG_DWORD)
        n = v.num;
    else if (v.type == REG_SZ)
        n = (DWORD)wcstoul(v.str.c_str(), nullptr, 0);
    else
        return false;

    return mode == 0 ? (n != 0) : (n == 0);
}

void addFinding(vector<Finding>& out, HKEY root, REGSAM view,
    const wstring& path, const wstring& name,
    const wstring& desc, const wstring& value,
    int fixKind = 0, DWORD fixNum = 0,
    const wstring& fixStr = L"")
{
    Finding f;
    f.root = root;
    f.view = view;
    f.path = path;
    f.name = name;
    f.desc = desc;
    f.valueText = value;
    f.fixKind = fixKind;
    f.fixNum = fixNum;
    f.fixStr = fixStr;
    out.push_back(f);
}

void scanSubkeyDebuggers(vector<Finding>& out, const wstring& basePath,
    REGSAM view, const wchar_t* valueName,
    const wstring& descPrefix)
{
    HKEY base = nullptr;

    if (regOpenKey(HKEY_LOCAL_MACHINE, basePath, KEY_READ, view, &base) != ERROR_SUCCESS)
        return;

    for (DWORD i = 0; ; ++i)
    {
        wchar_t sub[256] = {};
        DWORD len = 256;

        LONG r = RegEnumKeyExW(base, i, sub, &len, nullptr, nullptr,
            nullptr, nullptr);

        if (r == ERROR_NO_MORE_ITEMS)
            break;

        if (r != ERROR_SUCCESS)
            continue;

        wstring full = basePath + L"\\" + sub;

        RegVal v = regRead(HKEY_LOCAL_MACHINE, full.c_str(), valueName, view);

        if (v.found && (v.type == REG_SZ || v.type == REG_EXPAND_SZ) &&
            !trimW(v.str).empty())
        {
            addFinding(out, HKEY_LOCAL_MACHINE, view, full, valueName,
                descPrefix + sub, v.str);
        }
    }

    RegCloseKey(base);
}

void scanKeyValues(vector<Finding>& out, const wstring& path,
    const wstring& desc)
{
    HKEY key = nullptr;

    if (regOpenKey(HKEY_LOCAL_MACHINE, path, KEY_QUERY_VALUE, KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return;

    DWORD count = 0;
    DWORD maxName = 0;

    (void)RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &count, &maxName, nullptr, nullptr, nullptr);

    for (DWORD i = 0; i < count; ++i)
    {
        vector<wchar_t> nm((size_t)maxName + 2, 0);
        DWORD nameSize = maxName + 1;

        if (RegEnumValueW(key, i, nm.data(), &nameSize, nullptr, nullptr,
            nullptr, nullptr) != ERROR_SUCCESS)
            continue;

        wstring name(nm.data());
        RegVal v = regRead(HKEY_LOCAL_MACHINE, path.c_str(), name.c_str());

        addFinding(out, HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, path, name,
            desc + L": " + name, regText(v));
    }

    RegCloseKey(key);
}

#define PTH_SYS  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"
#define PTH_EXP  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"
#define PTH_DEF  L"SOFTWARE\\Policies\\Microsoft\\Windows Defender"
#define PTH_RTP  L"SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection"
#define PTH_FW   L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy"
#define PTH_NET  L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"
#define PTH_SC   L"SOFTWARE\\Microsoft\\Security Center"
#define PTH_WIN  L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"
#define PTH_SVC  L"SYSTEM\\CurrentControlSet\\Services\\"

vector<Finding> scanRestrictions()
{
    vector<Finding> found;

    static const PolicyCheck checks[] =
    {
        { PTH_SYS, L"DisableTaskMgr",       L"Блокировка Диспетчера задач",           0, 3, 0, 0, nullptr },
        { PTH_SYS, L"DisableRegistryTools", L"Блокировка редактора реестра",          0, 3, 0, 0, nullptr },
        { PTH_SYS, L"DisableLockWorkstation", L"Блокировка экрана (Win+L)",           0, 3, 0, 0, nullptr },
        { PTH_SYS, L"DisableChangePassword", L"Блокировка смены пароля",              0, 3, 0, 0, nullptr },
        { PTH_SYS, L"NoDispCPL",            L"Блокировка настроек экрана",            0, 3, 0, 0, nullptr },
        { PTH_SYS, L"EnableLUA",            L"Отключён контроль учётных записей (UAC)", 1, 2, 1, 1, nullptr },
        { PTH_SYS, L"ConsentPromptBehaviorAdmin", L"UAC повышает права без запроса",  1, 2, 1, 5, nullptr },
        { PTH_SYS, L"LocalAccountTokenFilterPolicy", L"Ослаблена защита UAC для удалённого доступа", 0, 2, 0, 0, nullptr },
        { L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", L"DisableCMD",
                                            L"Блокировка командной строки",           0, 3, 0, 0, nullptr },

                                            { PTH_EXP, L"NoRun",                L"Блокировка окна «Выполнить»",           0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoControlPanel",       L"Блокировка Панели управления",          0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoFolderOptions",      L"Блокировка параметров папок",           0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoViewContextMenu",    L"Блокировка контекстного меню",          0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoClose",              L"Блокировка завершения работы",          0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoLogOff",             L"Блокировка выхода из системы",          0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoDrives",             L"Скрытие дисков",                        0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoViewOnDrive",        L"Запрет доступа к дискам",               0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoDesktop",            L"Скрыт рабочий стол",                    0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoFind",               L"Блокировка поиска",                     0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoWinKeys",            L"Блокировка клавиш Win",                 0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoSetTaskbar",         L"Блокировка настроек панели задач",      0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoTrayContextMenu",    L"Блокировка меню панели задач",          0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoPropertiesMyComputer", L"Блокировка свойств «Этот компьютер»", 0, 3, 0, 0, nullptr },
                                            { PTH_EXP, L"NoManageMyComputerVerb", L"Блокировка «Управление компьютером»", 0, 3, 0, 0, nullptr },
                                            { L"SOFTWARE\\Policies\\Microsoft\\MMC", L"RestrictToPermittedSnapins",
                                                                                L"Блокировка консолей MMC",               0, 3, 0, 0, nullptr },

                                                                                { PTH_DEF, L"DisableAntiSpyware",   L"Microsoft Defender отключён политикой", 0, 2, 0, 0, nullptr },
                                                                                { PTH_DEF, L"DisableAntiVirus",     L"Антивирус Defender отключён политикой", 0, 2, 0, 0, nullptr },
                                                                                { PTH_RTP, L"DisableRealtimeMonitoring", L"Отключена защита в реальном времени", 0, 2, 0, 0, nullptr },
                                                                                { PTH_RTP, L"DisableBehaviorMonitoring", L"Отключён анализ поведения",        0, 2, 0, 0, nullptr },
                                                                                { PTH_RTP, L"DisableOnAccessProtection", L"Отключена защита при доступе",     0, 2, 0, 0, nullptr },
                                                                                { PTH_RTP, L"DisableIOAVProtection", L"Отключена проверка загрузок",          0, 2, 0, 0, nullptr },
                                                                                { PTH_SC,  L"AntiVirusDisableNotify", L"Скрыты уведомления антивируса",       0, 2, 0, 0, nullptr },
                                                                                { PTH_SC,  L"FirewallDisableNotify",  L"Скрыты уведомления брандмауэра",      0, 2, 0, 0, nullptr },
                                                                                { PTH_SC,  L"UpdatesDisableNotify",   L"Скрыты уведомления об обновлениях",   0, 2, 0, 0, nullptr },
                                                                                { PTH_FW L"\\StandardProfile", L"EnableFirewall", L"Брандмауэр отключён (частная сеть)", 1, 2, 1, 1, nullptr },
                                                                                { PTH_FW L"\\DomainProfile",   L"EnableFirewall", L"Брандмауэр отключён (домен)",        1, 2, 1, 1, nullptr },
                                                                                { PTH_FW L"\\PublicProfile",   L"EnableFirewall", L"Брандмауэр отключён (общедоступная сеть)", 1, 2, 1, 1, nullptr },
                                                                                { L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU", L"NoAutoUpdate",
                                                                                                                    L"Отключены автообновления Windows",      0, 2, 0, 0, nullptr },
                                                                                { L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate", L"DisableWindowsUpdateAccess",
                                                                                                                    L"Заблокирован доступ к Центру обновления", 0, 2, 0, 0, nullptr },
                                                                                { L"SOFTWARE\\Microsoft\\Windows Script Host\\Settings", L"Enabled",
                                                                                                                    L"Отключён Windows Script Host",          1, 3, 1, 1, nullptr },

                                                                                                                    { PTH_SVC L"wuauserv", L"Start", L"Отключена служба Центра обновления",       4, 2, 1, 3, nullptr },
                                                                                                                    { PTH_SVC L"BITS",     L"Start", L"Отключена служба фоновой передачи (BITS)", 4, 2, 1, 3, nullptr },
                                                                                                                    { PTH_SVC L"MpsSvc",   L"Start", L"Отключена служба брандмауэра",             4, 2, 1, 2, nullptr },

                                                                                                                    { PTH_NET, L"ProxyEnable",   L"Включён прокси-сервер (возможен перехват трафика)", 0, 1, 1, 0, nullptr },
                                                                                                                    { PTH_NET, L"AutoConfigURL", L"Задан скрипт автонастройки прокси (PAC)",   2, 3, 0, 0, nullptr },

                                                                                                                    { PTH_WIN, L"Load", L"Скрытый автозапуск (Windows\\Load)",                 2, 3, 0, 0, nullptr },
                                                                                                                    { PTH_WIN, L"Run",  L"Скрытый автозапуск (Windows\\Run)",                  2, 3, 0, 0, nullptr },

                                                                                                                    { L"SOFTWARE\\Classes\\.exe", L"", L"Подменена ассоциация .exe",           3, 2, 2, 0, L"exefile" },
                                                                                                                    { L"SOFTWARE\\Classes\\.bat", L"", L"Подменена ассоциация .bat",           3, 2, 2, 0, L"batfile" },
                                                                                                                    { L"SOFTWARE\\Classes\\.cmd", L"", L"Подменена ассоциация .cmd",           3, 2, 2, 0, L"cmdfile" },
                                                                                                                    { L"SOFTWARE\\Classes\\.com", L"", L"Подменена ассоциация .com",           3, 2, 2, 0, L"comfile" },
                                                                                                                    { L"SOFTWARE\\Classes\\.lnk", L"", L"Подменена ассоциация ярлыков (.lnk)", 3, 2, 2, 0, L"lnkfile" },
                                                                                                                    { L"SOFTWARE\\Classes\\.reg", L"", L"Подменена ассоциация .reg",           3, 2, 2, 0, L"regfile" },
                                                                                                                    { L"SOFTWARE\\Classes\\exefile\\shell\\open\\command", L"",
                                                                                                                                                        L"Подменён запуск .exe-файлов",        3, 2, 2, 0, L"\"%1\" %*" },
                                                                                                                    { L"Software\\Classes\\.exe", L"", L"Пользовательская подмена ассоциации .exe", 2, 1, 0, 0, nullptr },
                                                                                                                    { L"Software\\Classes\\.bat", L"", L"Пользовательская подмена ассоциации .bat", 2, 1, 0, 0, nullptr },
                                                                                                                    { L"Software\\Classes\\.lnk", L"", L"Пользовательская подмена ассоциации .lnk", 2, 1, 0, 0, nullptr },
                                                                                                                    { L"Software\\Classes\\exefile\\shell\\open\\command", L"",
                                                                                                                                                        L"Пользовательская подмена запуска .exe", 2, 1, 0, 0, nullptr }
    };

    HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };

    for (size_t c = 0; c < sizeof(checks) / sizeof(checks[0]); ++c)
    {
        for (int r = 0; r < 2; ++r)
        {
            if (!(checks[c].roots & (r == 0 ? 1 : 2)))
                continue;

            RegVal v = regRead(roots[r], checks[c].path, checks[c].name);

            if (!isBlocked(v, checks[c].mode, checks[c].fixStr))
                continue;

            addFinding(found, roots[r], KEY_WOW64_64KEY, checks[c].path,
                checks[c].name, checks[c].desc, regText(v),
                checks[c].fixKind, checks[c].fixNum,
                checks[c].fixStr ? checks[c].fixStr : L"");
        }
    }

    const wstring ifeo =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options";

    scanSubkeyDebuggers(found, ifeo, KEY_WOW64_64KEY, L"Debugger",
        L"Отладчик (IFEO) подменяет запуск: ");

    if (os64())
        scanSubkeyDebuggers(found, ifeo, KEY_WOW64_32KEY, L"Debugger",
            L"Отладчик (IFEO, x86) подменяет запуск: ");

    scanSubkeyDebuggers(found,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SilentProcessExit",
        KEY_WOW64_64KEY, L"MonitorProcess",
        L"Запуск программы при закрытии процесса: ");

    scanKeyValues(found,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCertDLLs",
        L"DLL внедряется во все процессы (AppCertDLLs)");

    const wchar_t* ae = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug";

    REGSAM aeViews[2] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY };

    for (int i = 0; i < (os64() ? 2 : 1); ++i)
    {
        RegVal v = regRead(HKEY_LOCAL_MACHINE, ae, L"Debugger", aeViews[i]);

        if (v.found && !trimW(v.str).empty())
            addFinding(found, HKEY_LOCAL_MACHINE, aeViews[i], ae, L"Debugger",
                i == 0 ? L"Системный отладчик сбоев (AeDebug)"
                : L"Системный отладчик сбоев (AeDebug, x86)", v.str);
    }

    return found;
}

bool fixFinding(const Finding& f)
{
    wstring id = rootName(f.root) + L"\\" + f.path + L"\\" + f.name;

    logw(L"Исправление ограничения: " + id + L" = " + f.valueText);

    regWriteStr(HKEY_CURRENT_USER,
        L"Software\\MyScannerRecovery\\RestrictionsBackup", id,
        KEY_WOW64_64KEY, f.valueText);

    if (f.fixKind == 1)
        return regWriteDword(f.root, f.path, f.name, f.view, f.fixNum);

    if (f.fixKind == 2)
        return regWriteStr(f.root, f.path, f.name, f.view, f.fixStr);

    return regDelete(f.root, f.path, f.name, f.view);
}

void restrictions()
{
    header(L"ПРОВЕРКА СИСТЕМНЫХ ОГРАНИЧЕНИЙ");

    if (!requireAdmin())
        return;

    wstring note;
    bool noteOk = true;

    while (true)
    {
        header(L"ПРОВЕРКА СИСТЕМНЫХ ОГРАНИЧЕНИЙ");

        color(THEME_TEXT);
        p(L"\n  Ищутся блокировки политик, подмена запуска, отладчики\n");
        p(L"  и отключение защиты Windows.\n");

        if (g_off.active)
        {
            color(THEME_ACCENT);
            p(L"  Офлайн-режим: проверяется " + g_off.winDir + L"\n");
            color(THEME_TEXT);
        }

        vector<Finding> found = scanRestrictions();

        color(THEME_TEXT);
        p(L"\n");

        if (found.empty())
        {
            healthIcon(H_OK);
            color(THEME_MAIN);
            p(L"Известных блокировок и подозрительных отладчиков не обнаружено.\n");
        }
        else
        {
            const size_t LIMIT = 14;

            for (size_t i = 0; i < found.size() && i < LIMIT; ++i)
            {
                color(THEME_ERROR);
                p(L"  [ ОБНАРУЖЕНО ] ");
                color(THEME_TEXT);
                p(clip(found[i].desc, 70) + L"\n");
                color(THEME_DIM);
                p(L"                 " + rootName(found[i].root) + L"\\...\\" +
                    (found[i].name.empty() ? wstring(L"(по умолчанию)") : found[i].name) + L" = " + clip(found[i].valueText, 55) + L"\n");
            }

            if (found.size() > LIMIT)
            {
                color(THEME_DIM);
                p(L"  ... и ещё: " + to_wstring(found.size() - LIMIT) + L"\n");
            }

            color(THEME_ERROR);
            p(L"\n  Найдено ограничений: " + to_wstring(found.size()) + L"\n");
        }

        if (!note.empty())
        {
            color(noteOk ? THEME_MAIN : THEME_ERROR);
            p(L"  " + note + L"\n");
            note.clear();
        }

        divider();

        color(THEME_DIM);

        if (found.empty())
            p(L"  [R] Проверить заново | [0] Назад\n");
        else
            p(L"  [F] Отключить все найденные | [R] Проверить заново | [0] Назад\n");

        color(THEME_MAIN);
        p(L"\n  Выбор: ");

        int k = readKey();
        p(L"\n");

        if (k == '0' || k == K_ESC)
            return;

        if (k == 'F' && !found.empty())
        {
            if (confirm(L"Найденные значения будут удалены (копия — в журнале и "
                L"реестре). Легитимные отладчики (Visual Studio, "
                L"Process Explorer) тоже будут отключены."))
            {
                int okCount = 0;
                int failCount = 0;

                for (size_t i = 0; i < found.size(); ++i)
                {
                    if (fixFinding(found[i]))
                        ++okCount;
                    else
                        ++failCount;
                }

                note = L"Исправлено: " + to_wstring(okCount) +
                    L", ошибок: " + to_wstring(failCount);
                noteOk = failCount == 0;

                logw(L"Сброс ограничений: исправлено " + to_wstring(okCount) +
                    L", ошибок " + to_wstring(failCount));
            }
        }
    }
}

// ============================================================
// АВТОЗАГРУЗКА (как в Диспетчере задач)
// ============================================================

struct StartupEntry
{
    bool    folder = false;
    HKEY    root = nullptr;
    REGSAM  view = KEY_WOW64_64KEY;
    wstring tag;
    wstring name;
    wstring command;
    DWORD   valType = REG_SZ;
    wstring source;
    HKEY    approvedRoot = nullptr;
    wstring approvedSub;
    wstring exePath;
    wstring publisher;
    bool    enabled = true;
    bool    exists = true;
    bool    suspicious = false;
    bool    uwp = false;
};

vector<StartupEntry> startupEntries;

const wchar_t* BACKUP_PATH = L"Software\\MyScannerRecovery\\StartupBackup";

wstring approvedPath(const wstring& sub)
{
    return L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
        L"StartupApproved\\" + sub;
}

wstring expandEnv(const wstring& s)
{
    wchar_t buf[2048] = {};
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), buf, 2048);

    if (n == 0 || n > 2048)
        return s;

    return wstring(buf);
}

wstring extractExePath(const wstring& raw)
{
    wstring c = trimW(expandEnv(raw));

    if (c.empty())
        return c;

    if (c[0] == L'"')
    {
        size_t e = c.find(L'"', 1);
        return e == wstring::npos ? c.substr(1) : c.substr(1, e - 1);
    }

    wstring low = lowerW(c);
    static const wchar_t* exts[] =
    { L".exe", L".com", L".bat", L".cmd", L".scr", L".lnk", L".vbs", L".js", L".ps1" };

    size_t best = wstring::npos;
    size_t bestLen = 0;

    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i)
    {
        size_t at = low.find(exts[i]);

        if (at != wstring::npos && (best == wstring::npos || at < best))
        {
            best = at;
            bestLen = wcslen(exts[i]);
        }
    }

    if (best != wstring::npos)
        return c.substr(0, best + bestLen);

    size_t sp = c.find(L' ');
    return sp == wstring::npos ? c : c.substr(0, sp);
}

wstring resolvePath(const wstring& path, bool& exists)
{
    exists = false;

    if (path.empty())
        return path;

    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        exists = true;
        return path;
    }

    wchar_t found[MAX_PATH] = {};

    if (path.find(L'\\') == wstring::npos &&
        SearchPathW(nullptr, path.c_str(), L".exe", MAX_PATH, found, nullptr))
    {
        exists = true;
        return wstring(found);
    }

    return path;
}

wstring filePublisherRaw(const wstring& path)
{
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);

    if (!size)
        return L"";

    vector<BYTE> data(size);

    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return L"";

    struct LangCp { WORD lang; WORD cp; };

    LangCp* tr = nullptr;
    UINT len = 0;

    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
        (LPVOID*)&tr, &len) || !tr || len < sizeof(LangCp))
        return L"";

    wchar_t sub[128] = {};
    swprintf_s(sub, L"\\StringFileInfo\\%04x%04x\\CompanyName",
        (unsigned)tr[0].lang, (unsigned)tr[0].cp);

    wchar_t* val = nullptr;
    UINT vlen = 0;

    if (VerQueryValueW(data.data(), sub, (LPVOID*)&val, &vlen) && val && vlen)
        return wstring(val);

    return L"";
}

wstring filePublisher(const wstring& path)
{
    static map<wstring, wstring> cache;

    map<wstring, wstring>::iterator it = cache.find(path);

    if (it != cache.end())
        return it->second;

    wstring v = filePublisherRaw(path);
    cache[path] = v;
    return v;
}

bool isApprovedEnabled(const StartupEntry& e)
{
    wstring path = approvedPath(e.approvedSub);
    HKEY key = nullptr;

    if (RegOpenKeyExW(e.approvedRoot, path.c_str(), 0,
        KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return true;

    BYTE data[16] = {};
    DWORD size = sizeof(data);
    DWORD type = 0;

    LONG r = RegQueryValueExW(key, e.name.c_str(), nullptr, &type, data, &size);
    RegCloseKey(key);

    if (r != ERROR_SUCCESS || type != REG_BINARY || size < 1)
        return true;

    return (data[0] & 1) == 0;
}

bool setStartupEnabled(StartupEntry& e, bool enable)
{
    wstring path = approvedPath(e.approvedSub);
    HKEY key = nullptr;

    if (RegCreateKeyExW(e.approvedRoot, path.c_str(), 0, nullptr, 0,
        KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;

    BYTE data[12] = {};

    if (enable)
    {
        data[0] = 0x02;
    }
    else
    {
        data[0] = 0x03;

        FILETIME ft{};
        GetSystemTimeAsFileTime(&ft);
        memcpy(data + 4, &ft, sizeof(ft));
    }

    LONG r = RegSetValueExW(key, e.name.c_str(), 0, REG_BINARY, data, sizeof(data));
    RegCloseKey(key);

    if (r == ERROR_SUCCESS)
    {
        e.enabled = enable;
        logw(wstring(enable ? L"Включена" : L"Отключена") +
            L" запись автозагрузки: " + e.name + L" (" + e.source + L")");
    }

    return r == ERROR_SUCCESS;
}

void finishEntry(StartupEntry& e)
{
    e.enabled = isApprovedEnabled(e);

    if (e.folder)
    {
        e.exePath = e.command;
        e.exists = true;
        e.publisher = L"—";
    }
    else
    {
        e.exePath = resolvePath(extractExePath(e.command), e.exists);

        e.publisher = e.exists ? filePublisher(e.exePath) : wstring();

        if (e.publisher.empty())
            e.publisher = e.exists ? L"Неизвестно" : L"Файл не найден";
    }

    wstring low = lowerW(e.exePath);

    e.suspicious = !e.exists ||
        low.find(L"\\temp\\") != wstring::npos ||
        (e.publisher == L"Неизвестно" && low.find(L"\\appdata\\") != wstring::npos);
}

void loadRunSource(HKEY root, REGSAM view, const wstring& tag,
    const wstring& label, const wstring& approvedSub)
{
    HKEY key = nullptr;

    if (RegOpenKeyExW(root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_QUERY_VALUE | view, &key) != ERROR_SUCCESS)
        return;

    DWORD count = 0;
    DWORD maxName = 0;
    DWORD maxData = 0;

    (void)RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, &count, &maxName, &maxData, nullptr, nullptr);

    for (DWORD i = 0; i < count; ++i)
    {
        vector<wchar_t> name((size_t)maxName + 2, 0);
        vector<BYTE> data((size_t)maxData + 4, 0);

        DWORD nameSize = maxName + 1;
        DWORD dataSize = maxData;
        DWORD type = 0;

        if (RegEnumValueW(key, i, name.data(), &nameSize, nullptr, &type,
            data.data(), &dataSize) != ERROR_SUCCESS)
            continue;

        if (type != REG_SZ && type != REG_EXPAND_SZ)
            continue;

        data[dataSize] = 0;
        data[dataSize + 1] = 0;

        StartupEntry e;
        e.folder = false;
        e.root = root;
        e.view = view;
        e.tag = tag;
        e.name = wstring(name.data());
        e.command = wstring((const wchar_t*)data.data());
        e.valType = type;
        e.source = label;
        e.approvedRoot = root;
        e.approvedSub = approvedSub;

        finishEntry(e);
        startupEntries.push_back(e);
    }

    RegCloseKey(key);
}

void loadFolderSource(int csidl, HKEY approvedRoot, const wstring& tag,
    const wstring& label)
{
    wchar_t dir[MAX_PATH] = {};

    if (FAILED(SHGetFolderPathW(nullptr, csidl, nullptr,
        SHGFP_TYPE_CURRENT, dir)))
        return;

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((wstring(dir) + L"\\*").c_str(), &fd);

    if (h == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        if (lowerW(fd.cFileName) == L"desktop.ini")
            continue;

        StartupEntry e;
        e.folder = true;
        e.root = approvedRoot;
        e.tag = tag;
        e.name = fd.cFileName;
        e.command = wstring(dir) + L"\\" + fd.cFileName;
        e.source = label;
        e.approvedRoot = approvedRoot;
        e.approvedSub = L"StartupFolder";

        finishEntry(e);
        startupEntries.push_back(e);
    } while (FindNextFileW(h, &fd));

    FindClose(h);
}

void loadUwpStartup()
{
    const wchar_t* base =
        L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\"
        L"CurrentVersion\\AppModel\\SystemAppData";

    HKEY key = nullptr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, base, 0,
        KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
        return;

    for (DWORD i = 0; ; ++i)
    {
        wchar_t pkg[256] = {};
        DWORD len = 256;

        LONG r = RegEnumKeyExW(key, i, pkg, &len, nullptr, nullptr, nullptr, nullptr);

        if (r == ERROR_NO_MORE_ITEMS)
            break;

        if (r != ERROR_SUCCESS)
            continue;

        wstring pkgPath = wstring(base) + L"\\" + pkg;
        HKEY pk = nullptr;

        if (RegOpenKeyExW(HKEY_CURRENT_USER, pkgPath.c_str(), 0,
            KEY_READ | KEY_WOW64_64KEY, &pk) != ERROR_SUCCESS)
            continue;

        for (DWORD j = 0; ; ++j)
        {
            wchar_t task[256] = {};
            DWORD tl = 256;

            LONG r2 = RegEnumKeyExW(pk, j, task, &tl, nullptr, nullptr, nullptr, nullptr);

            if (r2 == ERROR_NO_MORE_ITEMS)
                break;

            if (r2 != ERROR_SUCCESS)
                continue;

            RegVal st = regRead(HKEY_CURRENT_USER,
                (pkgPath + L"\\" + task).c_str(), L"State");

            if (!st.found || st.type != REG_DWORD || st.num > 4)
                continue;

            wstring shortName = pkg;
            size_t us = shortName.find(L'_');

            if (us != wstring::npos)
                shortName = shortName.substr(0, us);

            size_t dot = shortName.rfind(L'.');

            if (dot != wstring::npos)
                shortName = shortName.substr(dot + 1);

            StartupEntry e;
            e.uwp = true;
            e.tag = L"UWP";
            e.name = shortName;
            e.command = wstring(pkg) + L"\\" + task;
            e.source = L"Магазин (UWP)";
            e.publisher = L"Microsoft Store";
            e.exePath = e.command;
            e.exists = true;
            e.enabled = (st.num == 2 || st.num == 4);
            e.approvedRoot = HKEY_CURRENT_USER;
            startupEntries.push_back(e);
        }

        RegCloseKey(pk);
    }

    RegCloseKey(key);
}

wstring startupSig()
{
    wstring sig;

    for (size_t i = 0; i < startupEntries.size(); ++i)
        sig += startupEntries[i].tag + L"|" + startupEntries[i].name + L"|" +
        startupEntries[i].command + (startupEntries[i].enabled ? L"|1;" : L"|0;");

    return sig;
}

void loadStartup()
{
    startupEntries.clear();

    loadRunSource(HKEY_CURRENT_USER, KEY_WOW64_64KEY, L"HKCU", L"Реестр (HKCU)", L"Run");
    loadRunSource(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, L"HKLM", L"Реестр (HKLM)", L"Run");

    if (os64())
        loadRunSource(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, L"HKLM32",
            L"Реестр (x86)", L"Run32");

    loadFolderSource(CSIDL_STARTUP, HKEY_CURRENT_USER, L"FOLDER-USER", L"Папка (user)");
    loadFolderSource(CSIDL_COMMON_STARTUP, HKEY_LOCAL_MACHINE, L"FOLDER-ALL", L"Папка (all)");
    loadUwpStartup();

    sort(startupEntries.begin(), startupEntries.end(),
        [](const StartupEntry& a, const StartupEntry& b)
        {
            return CompareStringOrdinal(a.name.c_str(), -1,
                b.name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
}

bool deleteStartup(const StartupEntry& e)
{
    if (e.folder)
        return false;

    HKEY source = nullptr;

    if (RegOpenKeyExW(e.root, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE | e.view, &source) != ERROR_SUCCESS)
        return false;

    HKEY backup = nullptr;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, BACKUP_PATH, 0, nullptr, 0,
        KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &backup, nullptr) != ERROR_SUCCESS)
    {
        RegCloseKey(source);
        return false;
    }

    wstring backupName = e.tag + L"|" + e.name;

    LONG saved = RegSetValueExW(backup, backupName.c_str(), 0, e.valType,
        (const BYTE*)e.command.c_str(),
        (DWORD)((e.command.size() + 1) * sizeof(wchar_t)));

    RegCloseKey(backup);

    if (saved != ERROR_SUCCESS)
    {
        RegCloseKey(source);
        return false;
    }

    LONG deleted = RegDeleteValueW(source, e.name.c_str());
    RegCloseKey(source);

    if (deleted == ERROR_SUCCESS)
        logw(L"Удалена запись автозагрузки: " + e.name + L" = " + e.command);

    return deleted == ERROR_SUCCESS;
}

bool liveOnly(const wchar_t* feature)
{
    if (!g_off.active)
        return true;

    status(wstring(feature) + L" недоступно в офлайн-режиме. Отключите его в «Полезные функции».", false);
    pauseKey();
    return false;
}

void startup()
{
    if (!requireAdmin() || !liveOnly(L"Управление автозагрузкой"))
        return;

    int sel = 0;
    int top = 0;
    wstring message;
    bool messageOk = true;

    loadStartup();

    const int VISIBLE = max(5, min(22, g_rows - 27));

    while (true)
    {
        header(L"УПРАВЛЕНИЕ АВТОЗАГРУЗКОЙ");

        int total = (int)startupEntries.size();

        if (sel >= total) sel = total - 1;
        if (sel < 0) sel = 0;
        if (sel < top) top = sel;
        if (sel >= top + VISIBLE) top = sel - VISIBLE + 1;
        if (top < 0) top = 0;

        wchar_t windowsPath[MAX_PATH] = {};
        GetWindowsDirectoryW(windowsPath, MAX_PATH);

        color(THEME_DIM);
        p(L"\n  Текущая Windows: " + wstring(windowsPath) +
            L"  (другие установленные системы не сканируются)\n\n");

        if (total == 0)
        {
            color(THEME_MAIN);
            p(L"  Записей автозагрузки не найдено.\n");
        }
        else
        {
            color(THEME_ACCENT);
            p(L"      " + padR(L"Имя", 28) + L" " + padR(L"Издатель", 24) + L" " +
                padR(L"Состояние", 11) + L" " + L"Источник\n");

            color(THEME_DIM);
            p(L"  " + wstring(90, L'\u2500') + L"\n");

            for (int i = top; i < total && i < top + VISIBLE; ++i)
            {
                const StartupEntry& e = startupEntries[i];
                bool s = (i == sel);

                color(THEME_MAIN);
                p(s ? L" \u25BA " : L"   ");

                color(e.suspicious ? THEME_ERROR : (e.enabled ? THEME_MAIN : THEME_DIM));
                p(L"\u25A0 ");

                color(s ? THEME_TEXT : (e.enabled ? C_GRAY : THEME_DIM));
                p(padR(e.name, 28) + L" " + padR(e.publisher, 24) + L" ");

                color(e.enabled ? THEME_MAIN : THEME_DIM);
                p(padR(e.enabled ? L"Включено" : L"Отключено", 11) + L" ");

                color(THEME_DIM);
                p(clip(e.source, 14) + L"\n");
            }

            if (total > VISIBLE)
            {
                color(THEME_DIM);
                p(L"\n  Показано " + to_wstring(top + 1) + L"–" +
                    to_wstring(min(total, top + VISIBLE)) + L" из " +
                    to_wstring(total) + L"\n");
            }

            const StartupEntry& cur = startupEntries[sel];

            divider();

            color(THEME_ACCENT);
            p(L"  Команда : ");
            color(THEME_TEXT);
            p(clip(cur.command, 76) + L"\n");

            color(THEME_ACCENT);
            p(L"  Файл    : ");

            if (cur.exists)
            {
                color(THEME_TEXT);
                p(clip(cur.exePath, 76) + L"\n");
            }
            else
            {
                color(THEME_ERROR);
                p(L"НЕ НАЙДЕН" + (cur.exePath.empty() ? wstring()
                    : L" — " + clip(cur.exePath, 60)) + L"\n");
            }

            if (cur.suspicious)
            {
                color(THEME_ERROR);
                p(L"  ВНИМАНИЕ: запись выглядит подозрительно (нет файла или запуск из Temp/AppData).\n");
            }
        }

        if (!message.empty())
        {
            color(messageOk ? THEME_MAIN : THEME_ERROR);
            p(L"\n  " + message + L"\n");
            message.clear();
        }

        color(THEME_DIM);
        p(L"\n  \u2191\u2193 — выбор | ENTER/ПРОБЕЛ — вкл/выкл | D — удалить (с копией) | "
            L"O — Параметры | ESC — назад\n");

        int k = K_NONE;
        {
            eraseBelow();
            wstring prevSig = startupSig();
            wstring selKey = total ? startupEntries[sel].tag + L"|" + startupEntries[sel].name : wstring();
            ULONGLONG lastScan = GetTickCount64();

            for (;;)
            {
                if (_kbhit())
                {
                    k = readKeyRaw();
                    break;
                }

                Sleep(60);

                if (GetTickCount64() - lastScan > 1000)
                {
                    lastScan = GetTickCount64();
                    loadStartup();

                    if (startupSig() != prevSig)
                    {
                        for (size_t q = 0; q < startupEntries.size(); ++q)
                            if (startupEntries[q].tag + L"|" + startupEntries[q].name == selKey)
                                sel = (int)q;

                        message = L"Список автозагрузки обновлён.";
                        messageOk = true;
                        break;
                    }
                }
            }
        }

        if (k == K_ESC || k == '0')
            return;

        if (k == K_UP && total)
            sel = (sel + total - 1) % total;
        else if (k == K_DOWN && total)
            sel = (sel + 1) % total;
        else if (k == K_PGUP)
            sel = max(0, sel - VISIBLE);
        else if (k == K_PGDN)
            sel = min(max(0, total - 1), sel + VISIBLE);
        else if (k == K_HOME)
            sel = 0;
        else if (k == K_END)
            sel = max(0, total - 1);
        else if (k == 'R')
            loadStartup();
        else if (k == 'O')
            (void)ShellExecuteW(nullptr, L"open", L"ms-settings:startupapps",
                nullptr, nullptr, SW_SHOWNORMAL);
        else if ((k == K_ENTER || k == ' ') && total)
        {
            StartupEntry& e = startupEntries[sel];

            if (e.uwp)
            {
                message = L"Приложения из Магазина включаются в Параметрах (клавиша O).";
                messageOk = false;
                continue;
            }

            bool ok = setStartupEnabled(e, !e.enabled);

            message = ok ? (e.name + (e.enabled ? L": включено" : L": отключено"))
                : L"Не удалось изменить состояние записи.";
            messageOk = ok;
        }
        else if ((k == 'D' || k == K_DEL) && total)
        {
            StartupEntry e = startupEntries[sel];

            if (e.folder || e.uwp)
            {
                message = L"Эту запись можно только отключать/включать.";
                messageOk = false;
            }
            else if (confirm(L"Удалить запись " + e.name +
                L"? Команда сохранится, её можно вернуть в меню "
                L"«Восстановить автозагрузку»."))
            {
                bool ok = deleteStartup(e);

                message = ok ? L"Запись удалена." : L"Не удалось удалить запись.";
                messageOk = ok;

                if (ok)
                    loadStartup();
            }
        }
    }
}

// ============================================================
// ВОССТАНОВЛЕНИЕ УДАЛЁННОЙ АВТОЗАГРУЗКИ
// ============================================================

void restoreStartup()
{
    if (!liveOnly(L"Восстановление автозагрузки"))
        return;

    int sel = 0;
    wstring message;
    bool messageOk = true;

    while (true)
    {
        header(L"ВОССТАНОВЛЕНИЕ АВТОЗАГРУЗКИ");

        struct Backup
        {
            wstring valueName;
            wstring tag;
            wstring name;
            wstring command;
            DWORD type;
        };

        vector<Backup> list;

        HKEY key = nullptr;

        if (RegOpenKeyExW(HKEY_CURRENT_USER, BACKUP_PATH, 0,
            KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
        {
            DWORD count = 0;
            DWORD maxName = 0;
            DWORD maxData = 0;

            (void)RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, &count, &maxName, &maxData, nullptr, nullptr);

            for (DWORD i = 0; i < count; ++i)
            {
                vector<wchar_t> nm((size_t)maxName + 2, 0);
                vector<BYTE> data((size_t)maxData + 4, 0);

                DWORD nameSize = maxName + 1;
                DWORD dataSize = maxData;
                DWORD type = 0;

                if (RegEnumValueW(key, i, nm.data(), &nameSize, nullptr, &type,
                    data.data(), &dataSize) != ERROR_SUCCESS)
                    continue;

                if (type != REG_SZ && type != REG_EXPAND_SZ)
                    continue;

                data[dataSize] = 0;
                data[dataSize + 1] = 0;

                Backup b;
                b.valueName = wstring(nm.data());
                b.command = wstring((const wchar_t*)data.data());
                b.type = type;

                size_t bar = b.valueName.find(L'|');

                if (bar == wstring::npos)
                    continue;

                b.tag = b.valueName.substr(0, bar);
                b.name = b.valueName.substr(bar + 1);

                list.push_back(b);
            }

            RegCloseKey(key);
        }

        if (list.empty())
        {
            color(THEME_TEXT);
            p(L"\n  Резервных записей нет.\n");
            pauseKey();
            return;
        }

        if (sel >= (int)list.size()) sel = (int)list.size() - 1;
        if (sel < 0) sel = 0;

        color(THEME_TEXT);
        p(L"\n  Удалённые записи автозагрузки:\n\n");

        for (int i = 0; i < (int)list.size(); ++i)
        {
            bool s = (i == sel);

            color(s ? THEME_MAIN : THEME_DIM);
            p((s ? L"  \u25BA " : L"    ") +
                padR(list[i].name, 30) + L" [" + list[i].tag + L"]\n");

            if (s)
            {
                color(THEME_TEXT);
                p(L"        " + clip(list[i].command, 82) + L"\n");
            }
        }

        if (!message.empty())
        {
            color(messageOk ? THEME_MAIN : THEME_ERROR);
            p(L"\n  " + message + L"\n");
            message.clear();
        }

        color(THEME_DIM);
        p(L"\n  \u2191\u2193 — выбор | ENTER — восстановить | ESC — назад\n");

        int k = readKey();

        if (k == K_ESC || k == '0')
            return;

        if (k == K_UP)
            sel = (sel + (int)list.size() - 1) % (int)list.size();
        else if (k == K_DOWN)
            sel = (sel + 1) % (int)list.size();
        else if (k == K_ENTER)
        {
            if (!requireAdmin())
                continue;

            const Backup& b = list[sel];

            if (!confirm(L"Восстановить запись " + b.name + L"?"))
                continue;

            HKEY root = HKEY_CURRENT_USER;
            REGSAM view = KEY_WOW64_64KEY;
            wstring approved = L"Run";

            if (b.tag == L"HKLM")
                root = HKEY_LOCAL_MACHINE;
            else if (b.tag == L"HKLM32")
            {
                root = HKEY_LOCAL_MACHINE;
                view = KEY_WOW64_32KEY;
                approved = L"Run32";
            }

            bool ok = regWriteStr(root,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                b.name, view, b.command, b.type);

            if (ok)
            {
                StartupEntry tmp;
                tmp.approvedRoot = root;
                tmp.approvedSub = approved;
                tmp.name = b.name;
                setStartupEnabled(tmp, true);

                HKEY backup = nullptr;

                if (RegOpenKeyExW(HKEY_CURRENT_USER, BACKUP_PATH, 0,
                    KEY_SET_VALUE | KEY_WOW64_64KEY, &backup) == ERROR_SUCCESS)
                {
                    (void)RegDeleteValueW(backup, b.valueName.c_str());
                    RegCloseKey(backup);
                }

                logw(L"Восстановлена запись автозагрузки: " + b.name);
            }

            message = ok ? L"Запись восстановлена." : L"Не удалось восстановить запись.";
            messageOk = ok;
        }
    }
}

// ============================================================
// АВТОПРОВЕРКА СИСТЕМЫ И ЦЕНТР ПРОБЛЕМ
// ============================================================

enum FixKind
{
    FIX_NONE = 0,
    FIX_BOOT,
    FIX_RESTRICTIONS,
    FIX_SFC,
    FIX_FONTS,
    FIX_HOSTS,
    FIX_DEVICES,
    FIX_SERVICES,
    FIX_SAFEBOOT
};

struct Issue
{
    Health  level;
    wstring title;
    wstring detail;
    FixKind fix;
    wstring payload;
};

const int SCAN_STAGES = 6;

struct ScanShared
{
    volatile LONG pct;
    volatile LONG done;
    LONG          stageIssues[SCAN_STAGES];
    vector<Issue> issues;
};

ScanShared g_scan;
vector<Issue> g_issues;

bool readSmallFile(const wstring& path, wstring& text)
{
    text.clear();

    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};

    if (!GetFileSizeEx(h, &size) || size.QuadPart > 2 * 1024 * 1024)
    {
        CloseHandle(h);
        return false;
    }

    string bytes((size_t)size.QuadPart, '\0');
    DWORD got = 0;

    if (!bytes.empty() && !ReadFile(h, &bytes[0], (DWORD)bytes.size(), &got, nullptr))
        got = 0;

    CloseHandle(h);
    bytes.resize(got);

    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF &&
        (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF)
        bytes.erase(0, 3);

    text = fromUtf8(bytes);
    return true;
}

bool writeSmallFile(const wstring& path, const string& bytes)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    BOOL ok = bytes.empty() ? TRUE : WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
    CloseHandle(h);

    return ok && written == bytes.size();
}

wstring hostsPath()
{
    return targetSys32() + L"\\drivers\\etc\\hosts";
}

wstring fontsDir()
{
    return targetWinDir() + L"\\Fonts";
}

struct FontReg
{
    const wchar_t* regName;
    const wchar_t* file;
};

const FontReg CORE_FONTS[] =
{
    { L"Segoe UI (TrueType)",              L"segoeui.ttf" },
    { L"Segoe UI Bold (TrueType)",         L"segoeuib.ttf" },
    { L"Segoe UI Italic (TrueType)",       L"segoeuii.ttf" },
    { L"Segoe UI Light (TrueType)",        L"segoeuil.ttf" },
    { L"Segoe UI Semibold (TrueType)",     L"seguisb.ttf" },
    { L"Segoe UI Symbol (TrueType)",       L"seguisym.ttf" },
    { L"Segoe MDL2 Assets (TrueType)",     L"segmdl2.ttf" },
    { L"Arial (TrueType)",                 L"arial.ttf" },
    { L"Arial Bold (TrueType)",            L"arialbd.ttf" },
    { L"Arial Italic (TrueType)",          L"ariali.ttf" },
    { L"Times New Roman (TrueType)",       L"times.ttf" },
    { L"Courier New (TrueType)",           L"cour.ttf" },
    { L"Tahoma (TrueType)",                L"tahoma.ttf" },
    { L"Tahoma Bold (TrueType)",           L"tahomabd.ttf" },
    { L"Verdana (TrueType)",               L"verdana.ttf" },
    { L"Calibri (TrueType)",               L"calibri.ttf" },
    { L"Consolas (TrueType)",              L"consola.ttf" },
    { L"Lucida Console (TrueType)",        L"lucon.ttf" },
    { L"Marlett (TrueType)",               L"marlett.ttf" },
    { L"Symbol (TrueType)",                L"symbol.ttf" },
    { L"Wingdings (TrueType)",             L"wingding.ttf" }
};

int missingCoreFonts(vector<wstring>* names)
{
    int missing = 0;
    wstring dir = fontsDir();

    for (size_t i = 0; i < sizeof(CORE_FONTS) / sizeof(CORE_FONTS[0]); ++i)
    {
        if (!fileExists(dir + L"\\" + CORE_FONTS[i].file))
        {
            ++missing;

            if (names)
                names->push_back(CORE_FONTS[i].file);
        }
    }

    return missing;
}

bool isFontFile(const wstring& name)
{
    wstring low = lowerW(name);
    size_t dot = low.rfind(L'.');

    if (dot == wstring::npos)
        return false;

    wstring ext = low.substr(dot);
    return ext == L".ttf" || ext == L".ttc" || ext == L".otf" || ext == L".fon";
}

bool safeFileName(const wstring& name)
{
    if (name.empty() || name.size() > 120)
        return false;

    for (size_t i = 0; i < name.size(); ++i)
    {
        wchar_t c = name[i];

        if (c < 32 || c == L'\\' || c == L'/' || c == L':' || c == L'*' ||
            c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
            return false;
    }

    return name != L"." && name != L"..";
}

vector<wstring> findFontSources()
{
    vector<wstring> dirs;
    wstring sxs = targetWinDir() + L"\\WinSxS";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((sxs + L"\\*font*").c_str(), &fd);

    if (h == INVALID_HANDLE_VALUE)
        return dirs;

    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;

        wstring name = fd.cFileName;

        if (!safeFileName(name))
            continue;

        if (lowerW(name).find(L"font") != wstring::npos)
            dirs.push_back(sxs + L"\\" + name);
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return dirs;
}

int restoreFontFiles(int& failed)
{
    int copied = 0;
    failed = 0;

    wstring dest = fontsDir();
    vector<wstring> sources = findFontSources();

    for (size_t s = 0; s < sources.size(); ++s)
    {
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((sources[s] + L"\\*").c_str(), &fd);

        if (h == INVALID_HANDLE_VALUE)
            continue;

        do
        {
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
                continue;

            wstring name = fd.cFileName;

            if (!safeFileName(name) || !isFontFile(name))
                continue;

            wstring target = dest + L"\\" + name;

            if (fileExists(target))
                continue;

            if (CopyFileW((sources[s] + L"\\" + name).c_str(), target.c_str(), TRUE))
            {
                ++copied;
                logw(L"Восстановлен шрифт: " + name);
            }
            else
            {
                ++failed;
            }
        } while (FindNextFileW(h, &fd));

        FindClose(h);
    }

    return copied;
}

int restoreFontRegistry()
{
    int added = 0;
    const wstring key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    wstring dir = fontsDir();

    for (size_t i = 0; i < sizeof(CORE_FONTS) / sizeof(CORE_FONTS[0]); ++i)
    {
        if (!fileExists(dir + L"\\" + CORE_FONTS[i].file))
            continue;

        RegVal v = regRead(HKEY_LOCAL_MACHINE, key.c_str(), CORE_FONTS[i].regName);

        if (v.found && !trimW(v.str).empty())
            continue;

        if (regWriteStr(HKEY_LOCAL_MACHINE, key, CORE_FONTS[i].regName,
            KEY_WOW64_64KEY, CORE_FONTS[i].file))
            ++added;
    }

    return added;
}

void restoreFonts()
{
    header(L"ВОССТАНОВЛЕНИЕ ШРИФТОВ");

    if (!requireAdmin())
        return;

    vector<wstring> missing;
    int missingCount = missingCoreFonts(&missing);

    color(THEME_TEXT);
    p(L"\n  Шрифты копируются из хранилища компонентов Windows (WinSxS).\n");
    p(L"  Существующие файлы не заменяются и не удаляются.\n\n");

    if (missingCount == 0)
    {
        healthIcon(H_OK);
        color(THEME_MAIN);
        p(L"Основные системные шрифты на месте.\n");
    }
    else
    {
        healthIcon(H_BAD);
        color(THEME_ERROR);
        p(L"Отсутствуют основные шрифты: " + to_wstring(missingCount) + L"\n");

        color(THEME_DIM);
        wstring list;

        for (size_t i = 0; i < missing.size() && i < 8; ++i)
            list += (i ? L", " : L"") + missing[i];

        p(L"  " + list + (missing.size() > 8 ? L" ..." : L"") + L"\n");
    }

    if (!confirm(L"Восстановить недостающие шрифты из хранилища Windows?"))
        return;

    int failed = 0;
    int copied = restoreFontFiles(failed);
    int added = restoreFontRegistry();

    p(L"\n");
    status(L"Скопировано шрифтов: " + to_wstring(copied), failed == 0);
    status(L"Добавлено записей реестра: " + to_wstring(added), true);

    int left = missingCoreFonts(nullptr);

    if (left > 0)
    {
        color(C_YELLOW);
        p(L"\n  Часть шрифтов в хранилище не найдена (осталось: " + to_wstring(left) + L").\n");
        p(L"  Запустите «Проверка системных файлов (SFC)» и «DISM — восстановление».\n");
    }
    else
    {
        color(THEME_MAIN);
        p(L"\n  Готово. Для применения может потребоваться перезагрузка.\n");
    }

    logw(L"Восстановление шрифтов: скопировано " + to_wstring(copied) + L", записей " + to_wstring(added));
    pauseKey();
}

const char* DEFAULT_HOSTS =
"# Copyright (c) 1993-2009 Microsoft Corp.\r\n"
"#\r\n"
"# This is a sample HOSTS file used by Microsoft TCP/IP for Windows.\r\n"
"#\r\n"
"# localhost name resolution is handled within DNS itself.\r\n"
"#\t127.0.0.1       localhost\r\n"
"#\t::1             localhost\r\n";

bool hostsLineSuspicious(const wstring& line)
{
    static const wchar_t* words[] =
    {
        L"microsoft", L"windowsupdate", L"defender", L"kaspersky", L"eset", L"avast",
        L"avg.", L"drweb", L"malwarebytes", L"virustotal", L"norton", L"mcafee",
        L"bitdefender", L"symantec", L"sophos", L"comodo", L"avira"
    };

    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (line.find(words[i]) != wstring::npos)
            return true;

    return false;
}

int hostsSuspiciousCount(int* active)
{
    wstring text;
    int bad = 0;
    int act = 0;

    if (readSmallFile(hostsPath(), text))
    {
        text = lowerW(text);
        size_t start = 0;

        for (size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == L'\n')
            {
                wstring line = trimW(text.substr(start, i - start));
                start = i + 1;

                if (line.empty() || line[0] == L'#')
                    continue;

                ++act;

                if (hostsLineSuspicious(line))
                    ++bad;
            }
        }
    }

    if (active)
        *active = act;

    return bad;
}

wstring timeStamp()
{
    SYSTEMTIME t{};
    GetLocalTime(&t);

    wchar_t buf[32] = {};
    swprintf_s(buf, L"%04d%02d%02d-%02d%02d%02d", (int)t.wYear, (int)t.wMonth,
        (int)t.wDay, (int)t.wHour, (int)t.wMinute, (int)t.wSecond);

    return buf;
}

void resetHosts()
{
    header(L"СБРОС ФАЙЛА HOSTS");

    if (!requireAdmin())
        return;

    int active = 0;
    int bad = hostsSuspiciousCount(&active);

    color(THEME_TEXT);
    p(L"\n  Файл hosts может перенаправлять сайты антивирусов и обновлений.\n");
    p(L"  Перед сбросом создаётся резервная копия рядом с файлом.\n\n");

    p(L"  Активных записей: " + to_wstring(active) + L"\n");

    if (bad > 0)
    {
        color(THEME_ERROR);
        p(L"  Подозрительных записей: " + to_wstring(bad) + L"\n");
    }

    if (!confirm(L"Вернуть файл hosts к стандартному виду?"))
        return;

    wstring path = hostsPath();
    wstring backup = path + L".msr-" + timeStamp() + L".bak";

    if (fileExists(path))
    {
        (void)SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);

        if (!CopyFileW(path.c_str(), backup.c_str(), TRUE))
        {
            status(L"Не удалось создать резервную копию. Сброс отменён.", false);
            pauseKey();
            return;
        }
    }

    bool ok = writeSmallFile(path, DEFAULT_HOSTS);

    status(ok ? L"Файл hosts сброшен. Копия: " + backup : L"Не удалось записать файл hosts.", ok);
    logw(ok ? L"Сброшен hosts, копия " + backup : L"Ошибка сброса hosts");
    pauseKey();
}

struct ServiceFix
{
    const wchar_t* name;
    const wchar_t* title;
    DWORD          start;
};

const ServiceFix SERVICE_FIXES[] =
{
    { L"wuauserv",  L"Центр обновления Windows", 3 },
    { L"BITS",      L"Фоновая интеллектуальная служба передачи", 3 },
    { L"mpssvc",    L"Брандмауэр Windows", 2 },
    { L"EventLog",  L"Журнал событий Windows", 2 },
    { L"Winmgmt",   L"Инструментарий управления Windows", 2 }
};

int disabledServices(vector<int>* idx)
{
    int n = 0;

    for (size_t i = 0; i < sizeof(SERVICE_FIXES) / sizeof(SERVICE_FIXES[0]); ++i)
    {
        RegVal v = regRead(HKEY_LOCAL_MACHINE,
            (wstring(L"SYSTEM\\CurrentControlSet\\Services\\") + SERVICE_FIXES[i].name).c_str(),
            L"Start");

        if (v.found && v.type == REG_DWORD && v.num == 4)
        {
            ++n;

            if (idx)
                idx->push_back((int)i);
        }
    }

    return n;
}

void fixServices()
{
    header(L"ВАЖНЫЕ СЛУЖБЫ WINDOWS");

    if (!requireAdmin())
        return;

    vector<int> idx;
    int n = disabledServices(&idx);

    color(THEME_TEXT);
    p(L"\n  Вирусы часто отключают службы защиты и обновления.\n\n");

    for (size_t i = 0; i < sizeof(SERVICE_FIXES) / sizeof(SERVICE_FIXES[0]); ++i)
    {
        bool off = find(idx.begin(), idx.end(), (int)i) != idx.end();
        healthIcon(off ? H_BAD : H_OK);
        color(THEME_TEXT);
        p(padR(SERVICE_FIXES[i].title, 46) + (off ? L"отключена\n" : L"в норме\n"));
    }

    if (n == 0)
    {
        pauseKey();
        return;
    }

    if (confirm(L"Вернуть автоматический или ручной запуск для отключённых служб?"))
    {
        int ok = 0;

        for (size_t k = 0; k < idx.size(); ++k)
        {
            const ServiceFix& f = SERVICE_FIXES[idx[k]];

            if (regWriteDword(HKEY_LOCAL_MACHINE,
                wstring(L"SYSTEM\\CurrentControlSet\\Services\\") + f.name,
                L"Start", KEY_WOW64_64KEY, f.start))
                ++ok;
        }

        status(L"Исправлено служб: " + to_wstring(ok) + L" из " + to_wstring(n), ok == n);
        logw(L"Исправление служб: " + to_wstring(ok));
    }

    pauseKey();
}

bool bcdFlagOn(const wstring& text, const wchar_t* key)
{
    wstring low = lowerW(text);
    size_t at = low.find(key);

    if (at == wstring::npos)
        return false;

    size_t end = low.find(L'\n', at);
    wstring line = low.substr(at, end == wstring::npos ? wstring::npos : end - at);

    return line.find(L"yes") != wstring::npos || line.find(L"minimal") != wstring::npos ||
        line.find(L"network") != wstring::npos || line.find(L"alternateshell") != wstring::npos;
}

void scanBootAndKeys(vector<Issue>& out)
{
    vector<BootItem> items = makeBootItems();

    for (size_t i = 0; i < items.size(); ++i)
    {
        const BootItem& it = items[i];
        RegVal v = regRead(it.root, it.path.c_str(), it.name.c_str(), it.view);

        if (bootHealth(it, v) == H_OK)
            continue;

        Issue is;
        is.level = H_BAD;
        is.title = L"Изменено значение запуска: " + it.label;
        is.detail = L"Сейчас: " + clip(regText(v), 40) + L" | в норме: " + clip(bootDefaultText(it), 30);
        is.fix = FIX_BOOT;
        is.payload = it.label;
        out.push_back(is);
    }
}

void scanSystemFiles(vector<Issue>& out)
{
    static const wchar_t* files[] =
    {
        L"ntoskrnl.exe", L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"winlogon.exe",
        L"userinit.exe", L"services.exe", L"lsass.exe", L"smss.exe", L"csrss.exe",
        L"svchost.exe", L"cmd.exe", L"config\\SYSTEM", L"config\\SOFTWARE"
    };

    wstring dir = targetSys32();
    wstring missing;
    int count = 0;

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i)
    {
        if (!fileExists(dir + L"\\" + files[i]))
        {
            if (count < 4)
                missing += (count ? L", " : L"") + wstring(files[i]);

            ++count;
        }
    }

    if (!fileExists(targetWinDir() + L"\\explorer.exe"))
    {
        missing += (count ? L", " : L"") + wstring(L"explorer.exe");
        ++count;
    }

    if (count > 0)
    {
        Issue is;
        is.level = H_BAD;
        is.title = L"Отсутствуют системные файлы: " + to_wstring(count);
        is.detail = missing + (count > 4 ? L" ..." : L"");
        is.fix = FIX_SFC;
        out.push_back(is);
    }
}

void scanBcd(vector<Issue>& out)
{
    if (g_off.active || g_env.winpe)
        return;

    wstring text;
    DWORD code = 1;

    if (!captureProcess(sysExe(L"bcdedit.exe"), L"/enum {current}", text, 8000, &code) || code != 0)
        return;

    if (g_env.cleanBoot == 0 && bcdFlagOn(text, L"safeboot"))
    {
        Issue is;
        is.level = H_WARN;
        is.title = L"Включён обязательный запуск в безопасном режиме";
        is.detail = L"Windows будет загружаться в безопасном режиме при каждом старте.";
        is.fix = FIX_SAFEBOOT;
        out.push_back(is);
    }

    if (bcdFlagOn(text, L"nointegritychecks") || bcdFlagOn(text, L"testsigning"))
    {
        Issue is;
        is.level = H_WARN;
        is.title = L"В загрузчике отключена проверка подписей драйверов";
        is.detail = L"Включён testsigning или nointegritychecks. Так делают руткиты.";
        is.fix = FIX_NONE;
        out.push_back(is);
    }
}

void runScan(ScanShared& sh)
{
    sh.issues.clear();
    sh.done = 0;
    sh.pct = 0;

    for (int i = 0; i < SCAN_STAGES; ++i)
        sh.stageIssues[i] = 0;

    if (g_env.winpe && !g_off.active)
    {
        sh.pct = 100;
        sh.done = 1;
        return;
    }

    size_t before = 0;

    auto finish = [&](int stage)
        {
            sh.stageIssues[stage] = (LONG)(sh.issues.size() - before);
            before = sh.issues.size();
            sh.pct = (LONG)((stage + 1) * 100 / SCAN_STAGES);
        };

    scanBootAndKeys(sh.issues);
    finish(0);

    {
        vector<Finding> found = scanRestrictions();

        if (!found.empty())
        {
            Issue is;
            is.level = H_BAD;
            is.title = L"Найдены блокировки и подмены вирусов: " + to_wstring(found.size());
            is.detail = clip(found[0].desc, 70);
            is.fix = FIX_RESTRICTIONS;
            sh.issues.push_back(is);
        }
    }

    finish(1);

    scanSystemFiles(sh.issues);
    scanBcd(sh.issues);

    {
        int off = disabledServices(nullptr);

        if (off > 0)
        {
            Issue is;
            is.level = H_WARN;
            is.title = L"Отключены важные службы Windows: " + to_wstring(off);
            is.detail = L"Обновления, брандмауэр, журнал событий или WMI.";
            is.fix = FIX_SERVICES;
            sh.issues.push_back(is);
        }
    }

    finish(2);

    {
        int fonts = missingCoreFonts(nullptr);

        if (fonts > 0)
        {
            Issue is;
            is.level = H_BAD;
            is.title = L"Отсутствуют системные шрифты: " + to_wstring(fonts);
            is.detail = L"Интерфейс Windows может отображаться неправильно.";
            is.fix = FIX_FONTS;
            sh.issues.push_back(is);
        }

        int bad = hostsSuspiciousCount(nullptr);

        if (bad > 0)
        {
            Issue is;
            is.level = H_BAD;
            is.title = L"Файл hosts блокирует сайты защиты и обновлений";
            is.detail = L"Подозрительных записей: " + to_wstring(bad);
            is.fix = FIX_HOSTS;
            sh.issues.push_back(is);
        }
    }

    finish(3);

    if (!g_off.active)
    {
        vector<DevInfo> all = enumAllDevices();
        int bad = 0;

        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].problem != 0 && all[i].problem != CM_PROB_DISABLED)
                ++bad;

        if (bad > 0)
        {
            Issue is;
            is.level = H_WARN;
            is.title = L"Устройства с ошибками драйверов: " + to_wstring(bad);
            is.detail = L"Откройте проверку комплектующих для подробностей.";
            is.fix = FIX_DEVICES;
            sh.issues.push_back(is);
        }

        wstring drive = targetDrive() + L"\\";
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};

        if (GetDiskFreeSpaceExW(drive.c_str(), &freeBytes, &totalBytes, &totalFree) &&
            totalBytes.QuadPart > 0 && freeBytes.QuadPart * 100 / totalBytes.QuadPart < 5)
        {
            Issue is;
            is.level = H_WARN;
            is.title = L"Мало свободного места на системном диске";
            is.detail = L"Свободно " + fmtGB(freeBytes.QuadPart, 1) + L" ГБ. Windows может работать нестабильно.";
            is.fix = FIX_NONE;
            sh.issues.push_back(is);
        }
    }

    finish(4);
    finish(5);

    sh.pct = 100;
    sh.done = 1;
}

DWORD WINAPI scanThread(LPVOID)
{
    runScan(g_scan);
    return 0;
}

void rescanNow()
{
    runScan(g_scan);
    g_issues = g_scan.issues;
}

Health worstIssue()
{
    Health w = H_OK;

    for (size_t i = 0; i < g_issues.size(); ++i)
        if ((int)g_issues[i].level > (int)w)
            w = g_issues[i].level;

    return w;
}

bool isSafebootFixable()
{
    return !g_off.active && !g_env.winpe;
}

void fixSafeboot()
{
    header(L"РЕЖИМ ЗАГРУЗКИ");

    if (!requireAdmin())
        return;

    color(THEME_TEXT);
    p(L"\n  Будет снят флаг обязательного запуска в безопасном режиме.\n");

    if (confirm(L"Выполнить bcdedit /deletevalue {current} safeboot ?"))
    {
        RunResult r = runProcess(sysExe(L"bcdedit.exe"),
            L"/deletevalue {current} safeboot", L"BCDEDIT safeboot off");

        logw(L"Снят флаг safeboot: " + to_wstring((int)r));
    }

    pauseKey();
}

void applyFix(const Issue& is)
{
    switch (is.fix)
    {
    case FIX_BOOT:
    {
        if (!requireAdmin())
            return;

        vector<BootItem> items = makeBootItems();

        for (size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].label != is.payload)
                continue;

            header(L"ИСПРАВЛЕНИЕ ЗНАЧЕНИЯ ЗАПУСКА");
            color(THEME_TEXT);
            p(L"\n  " + items[i].label + L"\n  Стандартное значение: " + bootDefaultText(items[i]) + L"\n");

            if (confirm(L"Вернуть заводское значение?"))
            {
                bool ok = bootReset(items[i]);
                status(ok ? L"Значение восстановлено." : L"Не удалось восстановить значение.", ok);
                pauseKey();
            }

            return;
        }

        break;
    }
    case FIX_RESTRICTIONS: restrictions();   break;
    case FIX_SFC:          sfc();            break;
    case FIX_FONTS:        restoreFonts();   break;
    case FIX_HOSTS:        resetHosts();     break;
    case FIX_DEVICES:      hardwareMenu();   break;
    case FIX_SERVICES:     fixServices();    break;
    case FIX_SAFEBOOT:     fixSafeboot();    break;
    default:               break;
    }
}

wstring fixName(FixKind k)
{
    switch (k)
    {
    case FIX_BOOT:         return L"вернуть заводское значение";
    case FIX_RESTRICTIONS: return L"открыть проверку ограничений";
    case FIX_SFC:          return L"запустить проверку системных файлов";
    case FIX_FONTS:        return L"восстановить шрифты";
    case FIX_HOSTS:        return L"сбросить файл hosts";
    case FIX_DEVICES:      return L"открыть проверку комплектующих";
    case FIX_SERVICES:     return L"вернуть запуск служб";
    case FIX_SAFEBOOT:     return L"снять флаг безопасного режима";
    default:               return L"исправление вручную";
    }
}

void issueCenter()
{
    int sel = 0;

    for (;;)
    {
        header(L"ЦЕНТР ПРОБЛЕМ");

        if (g_issues.empty())
        {
            p(L"\n  ");
            healthIcon(H_OK);
            color(THEME_MAIN);
            p(L"Проблем не обнаружено.\n");
            color(THEME_DIM);
            p(L"\n  R — проверить заново | ESC — назад\n");

            int k = readKey();

            if (k == K_ESC || k == '0')
                return;

            if (k == 'R')
            {
                p(L"\n  Проверка...\n");
                rescanNow();
            }

            continue;
        }

        if (sel >= (int)g_issues.size())
            sel = (int)g_issues.size() - 1;

        color(THEME_TEXT);
        p(L"\n  Найдено проблем: " + to_wstring(g_issues.size()) + L"\n\n");

        for (int i = 0; i < (int)g_issues.size() && i < 12; ++i)
        {
            bool s = i == sel;

            color(s ? THEME_MAIN : THEME_TEXT);
            p(s ? L"  \u25BA " : L"    ");
            healthIcon(g_issues[i].level);
            color(s ? THEME_MAIN : THEME_TEXT);
            p(clip(g_issues[i].title, 74) + L"\n");

            if (s)
            {
                color(THEME_DIM);
                p(L"        " + clip(g_issues[i].detail, 80) + L"\n");
                color(THEME_ACCENT);
                p(L"        Действие: " + fixName(g_issues[i].fix) + L"\n");
            }
        }

        divider();
        color(THEME_DIM);
        p(L"  \u2191\u2193 — выбор | ENTER или F — исправить | R — проверить заново | ESC — назад\n");

        int k = readKey();

        if (k == K_ESC || k == '0')
            return;

        if (k == K_UP)
            sel = (sel + (int)g_issues.size() - 1) % (int)g_issues.size();
        else if (k == K_DOWN)
            sel = (sel + 1) % (int)g_issues.size();
        else if (k == 'R')
        {
            p(L"\n  Проверка...\n");
            rescanNow();
        }
        else if (k == K_ENTER || k == 'F')
        {
            Issue is = g_issues[sel];

            if (is.fix == FIX_NONE)
            {
                p(L"\n");
                status(L"Автоматического исправления нет. Исправьте вручную.", false);
                pauseKey();
            }
            else
            {
                applyFix(is);
                p(L"\n  Проверка...\n");
                rescanNow();
            }
        }
    }
}

// ============================================================
// РЕЖИМ РАБОТЫ: Windows, безопасный режим, WinRE
// ============================================================

void operatingMode()
{
    for (;;)
    {
        header(L"РЕЖИМ РАБОТЫ");

        color(THEME_TEXT);
        p(L"\n  Среда запуска:  ");
        color(THEME_MAIN);
        p(envName() + L"\n");

        color(THEME_TEXT);
        p(L"  Целевая система: ");
        color(THEME_ACCENT);
        p(g_off.active ? g_off.winDir + L" (офлайн)" : targetWinDir() + L" (текущая)");
        p(L"\n\n");

        vector<wstring> installs = findWindowsInstalls();

        if (!installs.empty())
        {
            color(THEME_TEXT);
            p(L"  Найденные установки Windows:\n");

            for (size_t i = 0; i < installs.size() && i < 6; ++i)
            {
                color(THEME_DIM);
                p(L"    [" + to_wstring(i + 1) + L"] " + installs[i] + L"\n");
            }
        }
        else if (g_env.winpe)
        {
            color(C_YELLOW);
            p(L"  Установленная Windows не найдена.\n");
        }

        divider();
        color(THEME_DIM);
        p(L"  [1-6] Подключить офлайн-систему   [U] Отключить офлайн-режим\n");

        if (!g_env.winpe)
        {
            p(L"  [S] Следующая загрузка: безопасный режим\n");
            p(L"  [N] Следующая загрузка: обычный режим\n");
            p(L"  [W] Перезагрузить в среду восстановления (WinRE)\n");
        }

        p(L"  [0] Назад\n");

        int k = readKey();

        if (k == '0' || k == K_ESC)
            return;

        if (k >= '1' && k <= '6' && (size_t)(k - '1') < installs.size())
        {
            if (!requireAdmin())
                continue;

            wstring win = installs[(size_t)(k - '1')];
            vector<wstring> users = findUserProfiles(win);
            wstring user = users.empty() ? wstring() : users[0];

            wstring error;

            if (mountOffline(win, user, error))
            {
                status(L"Подключена система " + win + (user.empty() ? L"" : L", профиль " + user), true);
                rescanNow();
            }
            else
            {
                status(error, false);
            }

            pauseKey();
        }
        else if (k == 'U')
        {
            unmountOffline();
            rescanNow();
        }
        else if (!g_env.winpe && (k == 'S' || k == 'N' || k == 'W'))
        {
            if (!requireAdmin() || g_off.active)
                continue;

            if (k == 'S')
            {
                if (confirm(L"Следующая загрузка будет в безопасном режиме."))
                    runProcess(sysExe(L"bcdedit.exe"), L"/set {current} safeboot minimal", L"BCDEDIT safeboot minimal");

                pauseKey();
            }
            else if (k == 'N')
            {
                if (confirm(L"Вернуть обычную загрузку Windows."))
                    runProcess(sysExe(L"bcdedit.exe"), L"/deletevalue {current} safeboot", L"BCDEDIT safeboot off");

                pauseKey();
            }
            else
            {
                if (confirm(L"Компьютер перезагрузится в среду восстановления Windows."))
                {
                    RunResult r = runProcess(sysExe(L"reagentc.exe"), L"/boottore", L"REAGENTC boottore");

                    if (r == RUN_OK)
                        runProcess(sysExe(L"shutdown.exe"), L"/r /t 5", L"SHUTDOWN restart");
                }

                pauseKey();
            }
        }
    }
}

// ============================================================
// ПОЛЕЗНЫЕ ФУНКЦИИ
// ============================================================

void showHiddenFiles()
{
    header(L"ПОКАЗАТЬ СКРЫТЫЕ ФАЙЛЫ И РАСШИРЕНИЯ");

    if (g_off.active)
    {
        status(L"Настройка относится к текущему пользователю и недоступна офлайн.", false);
        pauseKey();
        return;
    }

    color(THEME_TEXT);
    p(L"\n  Вирусы скрывают папки и файлы и прячут расширения.\n");
    p(L"  Будут включены скрытые файлы и показаны расширения.\n");

    if (confirm(L"Применить настройки проводника?"))
    {
        const wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";

        bool a = regWriteDword(HKEY_CURRENT_USER, key, L"Hidden", KEY_WOW64_64KEY, 1);
        bool b = regWriteDword(HKEY_CURRENT_USER, key, L"HideFileExt", KEY_WOW64_64KEY, 0);

        status(L"Настройки проводника изменены. Перезапустите проводник.", a && b);
    }

    pauseKey();
}

void unhideDriveFiles()
{
    header(L"ВЕРНУТЬ СКРЫТЫЕ ФАЙЛЫ НА НАКОПИТЕЛЕ");

    color(THEME_TEXT);
    p(L"\n  Вирусы на флешках скрывают папки и подменяют их ярлыками.\n");
    p(L"  Будут сняты атрибуты «скрытый» и «системный» с файлов накопителя.\n");
    p(L"  Файлы не удаляются. Применяется только к съёмным дискам.\n");

    wstring drive = readLine(L"\n  Буква съёмного диска: ");

    if (drive.size() < 1 || drive.size() > 2 || !iswalpha(drive[0]) || drive[0] > 127 ||
        (drive.size() == 2 && drive[1] != L':'))
    {
        status(L"Нужна одна буква диска, например E.", false);
        pauseKey();
        return;
    }

    wstring root = wstring(1, (wchar_t)towupper(drive[0])) + L":";

    if (GetDriveTypeW((root + L"\\").c_str()) != DRIVE_REMOVABLE)
    {
        status(L"Разрешены только съёмные накопители.", false);
        pauseKey();
        return;
    }

    if (confirm(L"Снять скрытые и системные атрибуты на диске " + root + L" ?"))
    {
        runProcess(sysExe(L"attrib.exe"), L"-h -s /s /d \"" + root + L"\\*\"", L"ATTRIB " + root);
        logw(L"Сняты атрибуты на " + root);
    }

    pauseKey();
}

void resetNetwork()
{
    header(L"СБРОС СЕТЕВЫХ НАСТРОЕК");

    if (!requireAdmin())
        return;

    if (g_off.active || g_env.winpe)
    {
        status(L"Недоступно в этом режиме.", false);
        pauseKey();
        return;
    }

    color(THEME_TEXT);
    p(L"\n  Сбрасываются Winsock, стек IP и кэш DNS. Нужна перезагрузка.\n");
    p(L"  Статические адреса и настройки Wi-Fi-профилей будут потеряны только\n");
    p(L"  для стека IP, сами профили сохраняются.\n");

    if (confirm(L"Выполнить сброс сети?"))
    {
        runProcess(sysExe(L"netsh.exe"), L"winsock reset", L"NETSH winsock reset");
        runProcess(sysExe(L"netsh.exe"), L"int ip reset", L"NETSH int ip reset");
        runProcess(sysExe(L"ipconfig.exe"), L"/flushdns", L"IPCONFIG flushdns");
    }

    pauseKey();
}

void createRestorePoint()
{
    header(L"ТОЧКА ВОССТАНОВЛЕНИЯ");

    if (!requireAdmin())
        return;

    if (g_off.active || g_env.winpe)
    {
        status(L"Недоступно в этом режиме.", false);
        pauseKey();
        return;
    }

    color(THEME_TEXT);
    p(L"\n  Создаёт точку восстановления перед исправлениями.\n");
    p(L"  Защита системы должна быть включена для диска Windows.\n");

    if (confirm(L"Создать точку восстановления?"))
        runPowerShell(L"Checkpoint-Computer -Description 'MyScanner Recovery' -RestorePointType MODIFY_SETTINGS",
            L"Точка восстановления");

    pauseKey();
}

void undoChanges()
{
    int sel = 0;
    wstring message;

    for (;;)
    {
        header(L"ОТКАТ ИЗМЕНЕНИЙ ПРОГРАММЫ");

        vector<JournalEntry> list = journalList();

        color(THEME_TEXT);
        p(L"\n  Каждое изменение реестра записывается и может быть отменено.\n\n");

        if (list.empty())
        {
            color(THEME_DIM);
            p(L"  Записей нет.\n");
        }

        if (sel >= (int)list.size())
            sel = list.empty() ? 0 : (int)list.size() - 1;

        for (int i = 0; i < (int)list.size() && i < 14; ++i)
        {
            color(i == sel ? THEME_MAIN : THEME_DIM);
            p((i == sel ? L"  \u25BA " : L"    ") + clip(journalDescribe(list[i]), 90) + L"\n");
        }

        if (!message.empty())
        {
            color(THEME_ACCENT);
            p(L"\n  " + message + L"\n");
            message.clear();
        }

        divider();
        color(THEME_DIM);
        p(L"  \u2191\u2193 — выбор | ENTER — откатить | ESC — назад\n");

        int k = readKey();

        if (k == K_ESC || k == '0')
            return;

        if (list.empty())
            continue;

        if (k == K_UP)
            sel = (sel + (int)list.size() - 1) % (int)list.size();
        else if (k == K_DOWN)
            sel = (sel + 1) % (int)list.size();
        else if (k == K_ENTER)
        {
            if (!requireAdmin())
                continue;

            if (confirm(L"Вернуть прежнее значение для выбранной записи?"))
                message = journalUndo(list[sel]) ? L"Изменение отменено." : L"Не удалось откатить запись.";
        }
    }
}

void saveReport()
{
    header(L"СОХРАНЕНИЕ ОТЧЁТА");

    wchar_t desktop[MAX_PATH] = {};

    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, desktop)) || !desktop[0])
    {
        wchar_t temp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, temp);
        wcscpy_s(desktop, temp);
    }

    wstring path = wstring(desktop) + L"\\MyScannerRecovery-report-" + timeStamp() + L".txt";

    wstring text = L"MyScanner Recovery " + wstring(APP_VERSION) + L"\r\n";
    text += L"Среда: " + envName() + L"\r\n";
    text += L"Система: " + (g_off.active ? g_off.winDir : targetWinDir()) + L"\r\n\r\n";

    if (g_issues.empty())
        text += L"Проблем не обнаружено.\r\n";

    for (size_t i = 0; i < g_issues.size(); ++i)
        text += (g_issues[i].level == H_BAD ? L"[ОШИБКА] " : L"[ВНИМАНИЕ] ") +
        g_issues[i].title + L"\r\n    " + g_issues[i].detail + L"\r\n";

    string bytes = "\xEF\xBB\xBF" + utf8(text);
    bool ok = writeSmallFile(path, bytes);

    status(ok ? L"Отчёт сохранён: " + path : L"Не удалось сохранить отчёт.", ok);
    pauseKey();
}

void usefulMenu()
{
    vector<wstring> names =
    {
        L"Восстановить шрифты Windows",
        L"Сбросить файл hosts",
        L"Вернуть важные службы Windows",
        L"Показать скрытые файлы и расширения",
        L"Вернуть скрытые файлы на флешке",
        L"Сбросить сетевые настройки",
        L"Создать точку восстановления",
        L"Откат изменений программы",
        L"Режим работы (WinRE, безопасный режим)",
        L"Проверка комплектующих",
        L"Сохранить отчёт автопроверки"
    };

    vector<wstring> descs =
    {
        L"Копирует недостающие шрифты из хранилища Windows",
        L"Резервная копия и возврат стандартного файла hosts",
        L"Обновления, брандмаузэр, журнал событий, WMI",
        L"Помогает увидеть то что возможно скрыл вирус",
        L"Снимает атрибуты скрытый и системный, файлы не удаляются",
        L"Winsock, стек IP и кэш DNS",
        L"Страховка перед исправлениями РЕКОМЕНДУЕТСЯ!!",
        L"Отмена любых изменений реестра, сделанных программой",
        L"Безопсаный режим, переход в WinRE",
        L"Память, процессор, диски и сеть",
        L"Отчет .txt с найденными проблемами"
    };

    int sel = 0;

    for (;;)
    {
        int pick = chooseMenu(L"ПОЛЕЗНЫЕ ФУНКЦИИ", L"Выберите действие:", names, descs, sel);

        switch (pick)
        {
        case 0:  restoreFonts();        break;
        case 1:  resetHosts();          break;
        case 2:  fixServices();         break;
        case 3:  showHiddenFiles();     break;
        case 4:  unhideDriveFiles();    break;
        case 5:  resetNetwork();        break;
        case 6:  createRestorePoint();  break;
        case 7:  undoChanges();         break;
        case 8:  operatingMode();       break;
        case 9:  hardwareMenu();        break;
        case 10: saveReport();          break;
        default: return;
        }

        rescanNow();
    }
}

// ============================================================
// СВЕДЕНИЯ О ПРОГРАММЕ
// ============================================================

void aboutProgram()
{
    header(L"СВЕДЕНИЯ О ПРОГРАММЕ");

    color(THEME_MAIN);
    p(L"\n  MyScanner Recovery\n");

    color(THEME_ACCENT);
    p(L"  VERSION " + wstring(APP_VERSION) + L"\n\n");

    color(THEME_TEXT);
    p(L"  MyScanner32 - средство для восстановления компьютера, от Kitarno, с поддержкой команды K-Studios\n");
    p(L"  На разработку ушло около одного месяца, программа написана на чистом C++\n");
    p(L"  Последняя версия все еще содержит баги, деняг на тестеров нету(\n");
    p(L"  На всю разработку ушло много времени и нервов, надеямся на вашу поддержку :3\n");
    p(L"  Связаться: kitarnodeveloper@gmail.com\n");
    p(L"  Я не несу ответственности за дальнейшие ваши изменения и дальнейшие модификации этой программы\n\n");

    color(THEME_DIM);
    p(L"  Среда запуска: " + envName() + L"\n");
    p(L"  Журнал: " + logPath() + L"\n");

    pauseKey();
}

// ============================================================
// ТЕРМИНАЛ (только белый список команд, без cmd /c со строкой пользователя)
// ============================================================

struct TermCmd
{
    const wchar_t* name;
    const wchar_t* exe;
    const wchar_t* args;
    const wchar_t* desc;
};

void terminal()
{
    static const TermCmd cmds[] =
    {
        { L"systeminfo", L"systeminfo.exe", L"",       L"сведения о Windows и компьютере" },
        { L"ipconfig",   L"ipconfig.exe",   L"/all",   L"сетевые настройки" },
        { L"hostname",   L"hostname.exe",   L"",       L"имя компьютера" },
        { L"whoami",     L"whoami.exe",     L"",       L"текущий пользователь" },
        { L"ver",        L"cmd.exe",        L"/d /c ver", L"версия Windows" },
        { L"tasklist",   L"tasklist.exe",   L"",       L"список процессов" },
        { L"chkdsk",     L"chkdsk.exe",     L"",       L"проверка файловой системы без исправления" }
    };

    while (true)
    {
        header(L"ВСТРОЕННЫЙ ТЕРМИНАЛ");

        p(L"\n");
        boxTop();
        boxLine(L"ДОСТУПНЫЕ КОМАНДЫ", THEME_MAIN, true);
        boxMid();

        for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
        {
            Segs row;
            row.push_back(make_pair(wstring(L" \u25BA "), THEME_MAIN));
            row.push_back(make_pair(padR(cmds[i].name, 12), THEME_ACCENT));
            row.push_back(make_pair(wstring(cmds[i].desc), THEME_TEXT));
            boxSeg(row, false);
        }

        {
            Segs row;
            row.push_back(make_pair(wstring(L" \u25BA "), THEME_MAIN));
            row.push_back(make_pair(padR(L"help", 12), THEME_ACCENT));
            row.push_back(make_pair(wstring(L"список команд"), THEME_TEXT));
            boxSeg(row, false);
        }

        {
            Segs row;
            row.push_back(make_pair(wstring(L" \u25BA "), THEME_MAIN));
            row.push_back(make_pair(padR(L"exit", 12), THEME_ACCENT));
            row.push_back(make_pair(wstring(L"выход"), THEME_TEXT));
            boxSeg(row, false);
        }

        boxBottom();
        color(THEME_DIM);
        p(L"\n  Только диагностика: команды из белого списка, ввод не передаётся в cmd.\n");

        divider();

        wstring command = lowerW(readLine(L"\n  MYSCANNER > "));

        if (command.empty())
            continue;

        if (command == L"exit")
            return;

        if (command == L"help")
        {
            color(THEME_TEXT);
            p(L"\n  Доступные команды перечислены выше.\n");
            pauseKey();
            continue;
        }

        bool matched = false;

        for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i)
        {
            if (command == cmds[i].name)
            {
                if (command == L"chkdsk" && !admin())
                {
                    status(L"Для chkdsk нужны права администратора.", false);
                }
                else
                {
                    runProcess(sysExe(cmds[i].exe), cmds[i].args, cmds[i].name);
                }

                matched = true;
                break;
            }
        }

        if (!matched)
        {
            color(THEME_ERROR);
            p(L"\n  Неизвестная или недоступная команда.\n");
        }

        pauseKey();
    }
}

// ============================================================
// КАСТОМИЗАЦИЯ
// ============================================================

void customization()
{
    int selected = currentTheme;
    int count = (int)themes.size();

    while (true)
    {
        header(L"КАСТОМИЗАЦИЯ");

        color(THEME_TEXT);
        p(L"\n  Выберите цветовую тему:\n\n");

        for (int i = 0; i < count; ++i)
        {
            color(i == selected ? themes[i].main : THEME_DIM);

            p((i == selected ? L"  \u25BA " : L"    ") +
                to_wstring(i + 1) + L". " + themes[i].name);

            if (i == currentTheme)
                p(L"  [АКТИВНА]");

            p(L"\n\n");
        }

        color(THEME_TEXT);
        p(L"  Иконки состояния: ");
        color(THEME_MAIN);
        p(useEmoji ? L"эмодзи  (\u2705 \u26A0\uFE0F \u274C)" : L"ASCII  ([OK] [!!] [XX])");
        p(L"\n");

        color(THEME_DIM);
        p(L"\n  \u2191\u2193 — выбор | ENTER — применить | I — сменить иконки | ESC — назад");

        int k = readKey();

        if (k == K_ESC)
            return;

        if (k == K_UP)
            selected = (selected + count - 1) % count;
        else if (k == K_DOWN)
            selected = (selected + 1) % count;
        else if (k == 'I')
        {
            useEmoji = !useEmoji;
            saveSettings();
        }
        else if (k == K_ENTER)
        {
            currentTheme = selected;
            applyTheme();
            saveSettings();

            logw(L"Выбрана тема: " + themes[currentTheme].name);
        }
    }
}

// ============================================================
// ЖУРНАЛ
// ============================================================

void showLog()
{
    header(L"ЖУРНАЛ ОПЕРАЦИЙ");

    vector<wstring> lines = readLogLines();

    if (lines.empty())
    {
        color(THEME_TEXT);
        p(L"\n  Журнал пока пуст.\n");
        pauseKey();
        return;
    }

    const size_t SHOW = 38;
    size_t from = lines.size() > SHOW ? lines.size() - SHOW : 0;

    color(THEME_DIM);
    p(L"\n  " + logPath() + L"\n\n");

    color(THEME_TEXT);

    for (size_t i = from; i < lines.size(); ++i)
        p(L"  " + clip(lines[i], 94) + L"\n");

    if (from > 0)
    {
        color(THEME_DIM);
        p(L"\n  Показаны последние " + to_wstring(SHOW) + L" строк из " +
            to_wstring(lines.size()) + L".\n");
    }

    pauseKey();
}

// ============================================================
// АНИМАЦИЯ ГЛАВНОГО МЕНЮ (плавные волны, один вызов вывода на кадр)
// ============================================================

void renderWave(int x0, int y0, int w, int h, double t)
{
    if (w <= 0 || h <= 0)
        return;

    vector<CHAR_INFO> buf((size_t)w * (size_t)h);

    static const wchar_t ramp[6] =
    { L' ', L'·', L'░', L'▒', L'▓', L'█' };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            double fx = x * 0.14;
            double fy = y * 0.85;

            double dx = x - w / 2.0;
            double dy = (y - h / 2.0) * 2.2;

            double v = sin(fx + t)
                + sin(fy - t * 0.8 + fx * 0.35)
                + sin(fx * 0.6 + fy * 0.5 + t * 1.3)
                + sin(sqrt(dx * dx * 0.04 + dy * dy * 0.3) - t * 1.7);

            double n = (v + 4.0) / 8.0;
            int lvl = (int)(n * 6.0);

            if (lvl < 0) lvl = 0;
            if (lvl > 5) lvl = 5;

            WORD attr = THEME_DIM;

            if (lvl == 3) attr = THEME_MAIN;
            else if (lvl == 4) attr = THEME_ACCENT;
            else if (lvl == 5) attr = C_WHITE;

            CHAR_INFO& c = buf[(size_t)y * (size_t)w + (size_t)x];
            c.Char.UnicodeChar = ramp[lvl];
            c.Attributes = attr;
        }
    }

    COORD size;
    size.X = (SHORT)w;
    size.Y = (SHORT)h;

    COORD from = { 0, 0 };

    SMALL_RECT rc;
    rc.Left = (SHORT)x0;
    rc.Top = (SHORT)y0;
    rc.Right = (SHORT)(x0 + w - 1);
    rc.Bottom = (SHORT)(y0 + h - 1);

    WriteConsoleOutputW(con, buf.data(), size, from, &rc);
}

// ============================================================
// ЗАСТАВКА С ЗАГРУЗКОЙ
// ============================================================

void intro()
{
    clear();
    showCursor(false);

    color(THEME_MAIN);

    p(L"\n\n");
    p(L"  __  __ __   __ ____   ____    _    _   _\n");
    p(L" |  \\/  |\\ \\ / // ___| / ___|  / \\  | \\ | |\n");
    p(L" | |\\/| | \\ V / \\___ \\| |     / _ \\ |  \\| |\n");
    p(L" | |  | |  | |   ___) | |___ / ___ \\| |\\  |\n");
    p(L" |_|  |_|  |_|  |____/ \\____/_/   \\_\\_| \\_|\n");

    color(THEME_TEXT);
    p(L"\n  \u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550  R E C O V E R Y  "
        L"\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\n");

    color(THEME_DIM);
    p(L"\n  Инициализация модулей...\n\n");

    COORD barPos = cursorPos();

    vector<wstring> stages =
    {
        L"Ключи запуска и загрузчик",
        L"Ограничения и политики",
        L"Системные файлы и службы",
        L"Шрифты и файл hosts",
        L"Устройства и накопители",
        L"Подготовка интерфейса"
    };

    const int W = 44;
    int shown = 0;
    static const wchar_t spin[] = L"|/-\\";

    g_scan.pct = 0;
    g_scan.done = 0;
    g_scan.issues.clear();

    HANDLE worker = CreateThread(nullptr, 0, scanThread, nullptr, 0, nullptr);

    if (!worker)
    {
        runScan(g_scan);
    }

    int i = 0;

    for (;;)
    {
        bool finished = g_scan.done != 0;
        int target = finished ? 100 : (int)g_scan.pct;

        if (i < target)
            ++i;

        if (finished && i >= 100)
            break;

        int filled = i * W / 100;

        gotoxy(2, barPos.Y);

        color(THEME_MAIN);
        p(wstring((size_t)max(0, filled - 1), L'\u2588'));

        if (filled > 0 && filled < W)
        {
            color(THEME_ACCENT);
            p(L"\u2593");
        }
        else if (filled >= W)
        {
            color(THEME_MAIN);
            p(L"\u2588");
        }

        if (filled < W)
        {
            color(THEME_DIM);
            p(L"\u2592");

            if (W - filled - 1 > 0)
                p(wstring((size_t)(W - filled - 1), L'\u2591'));
        }

        color(THEME_TEXT);
        wchar_t tail[32] = {};
        swprintf_s(tail, L" %3d%%  %c", i, i < 100 ? spin[(GetTickCount64() / 90) % 4] : L' ');
        p(tail);

        while (shown < (int)stages.size() && i >= (shown + 1) * 100 / (int)stages.size())
        {
            gotoxy(2, barPos.Y + 2 + shown);

            color(THEME_MAIN);
            p(L"\u25A0 ");
            color(THEME_TEXT);
            p(padR(stages[shown], 30));

            LONG found = g_scan.stageIssues[shown];

            if (found > 0)
            {
                color(THEME_ERROR);
                p(L"ПРОБЛЕМ: " + to_wstring(found));
            }
            else
            {
                color(THEME_MAIN);
                p(L"OK");
            }

            ++shown;
        }

        Sleep(g_scan.done || i < target ? 12 : 40);
    }

    if (worker)
    {
        (void)WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
    }

    g_issues = g_scan.issues;

    gotoxy(2, barPos.Y + 2 + (int)stages.size() + 1);

    if (g_issues.empty())
    {
        color(THEME_MAIN);
        p(L"Проблем не обнаружено. Запуск интерфейса...\n");
    }
    else
    {
        color(THEME_ERROR);
        p(L"Обнаружено проблем: " + to_wstring(g_issues.size()) + L". Запуск интерфейса...\n");
    }

    Sleep(450);
}

// ============================================================
// ВЫХОД: отдельное меню + плавная анимация
// ============================================================

void boxLineAt(int y, const wstring& text, WORD c)
{
    gotoxy(0, y);
    boxLine(text, c, true);
}

void boxBarAt(int y, int pct)
{
    const int BW = 40;
    int filled = pct * BW / 100;

    wchar_t num[16] = {};
    swprintf_s(num, L" %3d%%", pct);

    Segs s;
    s.push_back(make_pair(wstring((size_t)filled, L'\u2588'), THEME_MAIN));
    s.push_back(make_pair(wstring((size_t)(BW - filled), L'\u2591'), THEME_DIM));
    s.push_back(make_pair(wstring(num), THEME_TEXT));

    gotoxy(0, y);
    boxSeg(s, true);
}

void printCentered(int y, const wstring& text, WORD c)
{
    int x = (CON_COLS - (int)text.size()) / 2;

    if (x < 0)
        x = 0;

    gotoxy(x, y);
    color(c);
    p(text);
}

void exitAnimation()
{
    clear();
    showCursor(false);

    const int top = 14;

    gotoxy(0, top);

    boxTop();
    boxLine(L"ЗАВЕРШЕНИЕ РАБОТЫ", THEME_MAIN, true);
    boxMid();
    boxLine(L"", THEME_TEXT);

    int stageY = top + 4;
    boxLine(L"", THEME_TEXT);

    boxLine(L"", THEME_TEXT);

    int barY = top + 6;
    boxLine(L"", THEME_TEXT);
    boxLine(L"", THEME_TEXT);
    boxBottom();

    const wchar_t* stages[3] =
    {
        L"Сохранение журнала операций...",
        L"Закрытие модулей диагностики...",
        L"Освобождение ресурсов..."
    };

    const double PI = 3.14159265358979;
    int lastStage = -1;

    logw(L"MyScanner Recovery завершает работу.");

    for (int pct = 100; pct >= 0; --pct)
    {
        int idx = (100 - pct) * 3 / 101;

        if (idx != lastStage)
        {
            boxLineAt(stageY, stages[idx], THEME_TEXT);
            lastStage = idx;
        }

        boxBarAt(barY, pct);

        Sleep((DWORD)(5 + 12 * sin((100 - pct) * PI / 100.0)));
    }

    Sleep(150);

    clear();

    wstring bye = L"До новых встреч, " + currentUser + L"!";
    wstring sub = L"MyScanner Recovery завершил работу";

    WORD frames[] =
    {
        THEME_DIM, THEME_MAIN, C_WHITE, C_WHITE, C_WHITE,
        THEME_MAIN, THEME_DIM
    };

    for (size_t f = 0; f < sizeof(frames) / sizeof(frames[0]); ++f)
    {
        printCentered(22, bye, frames[f]);
        printCentered(24, sub, THEME_DIM == frames[f] ? THEME_DIM : THEME_ACCENT);
        Sleep(140);
    }

    printCentered(22, wstring(bye.size(), L' '), 0);
    printCentered(24, wstring(sub.size(), L' '), 0);

    Sleep(150);

    clear();
    color(C_GRAY);
}

bool exitMenu()
{
    int sel = 0;

    while (true)
    {
        home();
        showCursor(false);

        p(L"\n\n\n\n\n\n\n\n");

        boxTop();
        boxLine(L"ВЫХОД ИЗ ПРОГРАММЫ", THEME_MAIN, true);
        boxMid();
        boxLine(L"", THEME_TEXT);
        boxLine(L"Завершить работу MyScanner Recovery?", THEME_TEXT, true);
        boxLine(L"Сеанс пользователя: " + currentUser, THEME_DIM, true);
        boxLine(L"", THEME_TEXT);

        const size_t BTN = 20;

        wstring labels[2] = { L"ВЫЙТИ", L"ОСТАТЬСЯ" };

        Segs top, mid, bot;

        for (int b = 0; b < 2; ++b)
        {
            bool s = (b == sel);
            WORD c = s ? THEME_MAIN : THEME_DIM;

            wstring lt = s ? L"\u2554" : L"\u250C";
            wstring rt = s ? L"\u2557" : L"\u2510";
            wstring lb = s ? L"\u255A" : L"\u2514";
            wstring rb = s ? L"\u255D" : L"\u2518";
            wchar_t hl = s ? L'\u2550' : L'\u2500';
            wstring vl = s ? L"\u2551" : L"\u2502";

            top.push_back(make_pair(lt + wstring(BTN - 2, hl) + rt, c));
            mid.push_back(make_pair(vl + centerIn(labels[b], BTN - 2) + vl,
                s ? C_WHITE : THEME_DIM));
            bot.push_back(make_pair(lb + wstring(BTN - 2, hl) + rb, c));

            if (b == 0)
            {
                top.push_back(make_pair(wstring(4, L' '), c));
                mid.push_back(make_pair(wstring(4, L' '), c));
                bot.push_back(make_pair(wstring(4, L' '), c));
            }
        }

        boxSeg(top, true);
        boxSeg(mid, true);
        boxSeg(bot, true);

        boxLine(L"", THEME_TEXT);
        boxBottom();

        color(THEME_DIM);
        p(L"\n  \u2190\u2192 — выбор | ENTER — подтвердить | ESC — остаться\n");

        int k = readKey();

        if (k == K_LEFT || k == K_RIGHT || k == '\t')
            sel = 1 - sel;
        else if (k == 'Y')
        {
            sel = 0;
            break;
        }
        else if (k == K_ESC || k == 'N')
            return false;
        else if (k == K_ENTER)
        {
            if (sel == 1)
                return false;

            break;
        }
    }

    exitAnimation();
    return true;
}

// ============================================================
// ГЛАВНОЕ МЕНЮ
// ============================================================

const vector<wstring>& menuItems()
{
    static const vector<wstring> v =
    {
        L"Информация о системе",
        L"Проверка системных файлов (SFC)",
        L"Восстановление Windows (DISM)",
        L"Диагностика загрузчика",
        L"Убрать известные ограничения",
        L"Управление автозагрузкой",
        L"Восстановить автозагрузку",
        L"Полезные функции",
        L"Встроенный терминал",
        L"Кастомизация",
        L"Журнал операций",
        L"Сведения о программе",
        L"Выход"
    };

    return v;
}

const vector<wstring>& menuDescs()
{
    static const vector<wstring> v =
    {
        L"Полная конфигурация системы",
        L"Проверка целостности системных файлов Windows",
        L"Диагностика и восстановление хранилища компонентов",
        L"Userinit, Shell, BootExecute, AppInit и конфигурация загрузчика BCD",
        L"Блокировки, отладчики, подмена запуска и отключение UAC",
        L"Управляйте автозагрузкой",
        L"Возврат ранее удалённых записей автозагрузки",
        L"Полезные утилиты, обязательно загляните",
        L"Кастомный терминал с небольшим количеством функций (БЕТА)",
        L"Цветовые темы и стиль иконок",
        L"История операций программы",
        L"Информация о программе и связь с разработчиком",
        L"Закрыть MyScanner Recovery :("
    };

    return v;
}

void drawAlertPanel()
{
    p(L"\n");

    if (g_issues.empty())
    {
        p(L"  ");
        healthIcon(H_OK);
        color(THEME_MAIN);
        p(L"Автопроверка: проблем не найдено\n");
        p(L"\n\n\n");
        return;
    }

    Health worst = worstIssue();

    p(L"  ");
    healthIcon(worst);
    color(worst == H_BAD ? THEME_ERROR : C_YELLOW);
    p(L"ВНИМАНИЕ: обнаружены проблемы (" + to_wstring(g_issues.size()) + L")\n");

    for (size_t i = 0; i < 2; ++i)
    {
        if (i < g_issues.size())
        {
            color(THEME_TEXT);
            p(L"     \u2022 " + clip(g_issues[i].title, 80) + L"\n");
        }
        else
        {
            p(L"\n");
        }
    }

    color(THEME_ACCENT);

    if (g_issues.size() > 2)
        p(L"     и ещё: " + to_wstring(g_issues.size() - 2) + L"   |   ");
    else
        p(L"     ");

    p(L"F — открыть центр проблем и исправить\n");
}

void refreshIssues()
{
    home();
    color(THEME_DIM);
    p(L"\n  Обновление автопроверки...");
    rescanNow();
}

void drawMenuItems(int selected)
{
    const vector<wstring>& items = menuItems();
    const vector<wstring>& descs = menuDescs();

    for (int i = 0; i < (int)items.size(); ++i)
    {
        wstring num = (i + 1 < 10 ? L" " : L"") + to_wstring(i + 1);

        if (i == selected)
        {
            color(THEME_MAIN);
            p(L"  ► " + num + L". " + items[i] + L"\n");

            color(THEME_TEXT);
            p(L"         " + clip(descs[i], 84) + L"\n");
        }
        else
        {
            color(THEME_DIM);
            p(L"    " + num + L". " + items[i] + L"\n");
        }
    }
}

void drawMenuStatic(int selected, int& itemsTop, int& animTop, int& animH)
{
    clear();
    showCursor(false);

    p(L"\n");
    boxTop();
    boxLine(L"", THEME_MAIN);
    boxLine(L"███╗   ███╗██╗   ██╗███████╗ ██████╗ █████╗ ███╗   ██╗", THEME_MAIN, true);
    boxLine(L"████╗ ████║╚██╗ ██╔╝██╔════╝██╔════╝██╔══██╗████╗  ██║", THEME_MAIN, true);
    boxLine(L"██╔████╔██║ ╚████╔╝ ███████╗██║     ███████║██╔██╗ ██║", THEME_MAIN, true);
    boxLine(L"██║╚██╔╝██║  ╚██╔╝  ╚════██║██║     ██╔══██║██║╚██╗██║", THEME_MAIN, true);
    boxLine(L"██║ ╚═╝ ██║   ██║   ███████║╚██████╗██║  ██║██║ ╚████║", THEME_MAIN, true);
    boxLine(L"╚═╝     ╚═╝   ╚═╝   ╚══════╝ ╚═════╝╚═╝  ╚═╝╚═╝  ╚═══╝", THEME_MAIN, true);
    boxLine(L"", THEME_MAIN);
    boxLine(L"R  E  C  O  V  E  R  Y", THEME_TEXT, true);
    boxMid();

    {
        Segs s;
        s.push_back(make_pair(wstring(L"Добро пожаловать, "), THEME_TEXT));
        s.push_back(make_pair(currentUser, THEME_ACCENT));
        s.push_back(make_pair(wstring(L"!"), THEME_TEXT));
        boxSeg(s, true);
    }

    boxLine(L"VERSION " + wstring(APP_VERSION), THEME_DIM, true);
    boxBottom();

    color(THEME_TEXT);
    p(L"\n  СТАТУС: ");
    color(THEME_MAIN);
    p(L"АКТИВЕН");

    color(THEME_TEXT);
    p(L"      ПРАВА: ");
    color(admin() ? THEME_MAIN : THEME_ERROR);
    p(admin() ? L"АДМИНИСТРАТОР\n" : L"ОБЫЧНЫЙ ПОЛЬЗОВАТЕЛЬ\n");

    drawAlertPanel();

    color(THEME_DIM);
    p(L"\n  " + wstring(24, L'─') + L" ГЛАВНОЕ МЕНЮ " +
        wstring(24, L'─') + L"\n\n");

    itemsTop = cursorPos().Y;

    drawMenuItems(selected);

    animTop = cursorPos().Y + 1;

    animH = g_rows - animTop - 3;

    if (animH > 7) animH = 7;
    if (animH < 0) animH = 0;

    gotoxy(0, animTop + animH + (animH ? 1 : 0));

    color(THEME_MAIN);
    p(L"  " + wstring(BOX_W + 2, L'═') + L"\n");

    color(THEME_DIM);
    p(L"  ↑↓ — НАВИГАЦИЯ | ENTER — ВЫБРАТЬ | ESC — ВЫХОД\n");
}

void menu()
{
    int selected = 0;
    int count = (int)menuItems().size();

    int itemsTop = 0;
    int animTop = 0;
    int animH = 0;

    drawMenuStatic(selected, itemsTop, animTop, animH);

    while (true)
    {
        if (animH > 0)
            renderWave(g_margin + 2, animTop, BOX_W + 2, animH,
                GetTickCount64() / 1000.0 * 0.9);

        if (!_kbhit())
        {
            Sleep(33);
            continue;
        }

        int k = readKeyRaw();
        int old = selected;
        bool redrawAll = false;

        if (k == K_UP)
            selected = (selected + count - 1) % count;
        else if (k == K_DOWN)
            selected = (selected + 1) % count;
        else if (k == K_HOME)
            selected = 0;
        else if (k == K_END)
            selected = count - 1;
        else if (k == K_ENTER)
        {
            switch (selected)
            {
            case 0:  systemInfo();     break;
            case 1:  sfc();            break;
            case 2:  dism();           break;
            case 3:  bootDiag();       break;
            case 4:  restrictions();   break;
            case 5:  startup();        break;
            case 6:  restoreStartup(); break;
            case 7:  usefulMenu();     break;
            case 8:  terminal();       break;
            case 9:  customization();  break;
            case 10: showLog();        break;
            case 11: aboutProgram();   break;
            case 12:
                if (exitMenu())
                    return;
                break;
            }

            if (selected != 12)
                refreshIssues();

            redrawAll = true;
        }
        else if (k == 'F' && !g_issues.empty())
        {
            issueCenter();
            refreshIssues();
            redrawAll = true;
        }
        else if (k == K_ESC)
        {
            if (exitMenu())
                return;

            redrawAll = true;
        }

        if (redrawAll)
        {
            drawMenuStatic(selected, itemsTop, animTop, animH);
        }
        else if (selected != old)
        {
            gotoxy(0, itemsTop);
            drawMenuItems(selected);
        }
    }
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    (void)SetConsoleTitleW(L"MYSCANNER RECOVERY");

    (void)SetConsoleOutputCP(CP_UTF8);
    (void)SetConsoleCP(CP_UTF8);

    (void)SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    (void)SetDllDirectoryW(L"");
    (void)SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    (void)SetConsoleCtrlHandler(ctrlHandler, TRUE);

    wchar_t userName[UNLEN + 1] = {};
    DWORD userSize = UNLEN + 1;

    if (GetUserNameW(userName, &userSize) && userName[0])
        currentUser = userName;

    loadSettings();
    applyTheme();
    setupConsole();

    if (!admin())
    {
        (void)elevate();
        return 0;
    }

    HANDLE instance = CreateMutexW(nullptr, TRUE, L"Local\\MyScannerRecovery_Instance");

    if (instance && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        clear();
        color(THEME_ERROR);
        p(L"\n  MyScanner Recovery уже запущен.\n");
        Sleep(1500);
        CloseHandle(instance);
        return 0;
    }

    detectEnvironment();

    if (g_env.winpe)
    {
        vector<wstring> installs = findWindowsInstalls();

        if (!installs.empty())
        {
            vector<wstring> users = findUserProfiles(installs[0]);
            wstring error;

            if (!mountOffline(installs[0], users.empty() ? wstring() : users[0], error))
                logw(L"Автоподключение офлайн-системы не удалось: " + error);
        }
    }

    logw(L"MyScanner Recovery v" + wstring(APP_VERSION) + L" запущен. Среда: " + envName() +
        L". Пользователь: " + currentUser);

    intro();
    menu();

    unmountOffline();

    if (instance)
        CloseHandle(instance);

    return 0;
}
ЫЫ
