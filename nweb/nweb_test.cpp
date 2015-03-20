#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "nweb_test.h"


static void WideToUTF8(const wchar_t* src, size_t len, std::string& out)
{
    size_t tmp_len = ::WideCharToMultiByte(CP_UTF8, 0, src, len, 0, 0, 0, 0);
    char* buf = new char[tmp_len+1];
    buf[tmp_len] = 0;
    ::WideCharToMultiByte(CP_UTF8, 0, src, len, buf, tmp_len+1, 0, 0);
    out = buf;
    delete[] buf;
}

static void UTF8ToWide(const char* src, size_t len, std::wstring& out)
{
    size_t tmp_len = ::MultiByteToWideChar(CP_UTF8, 0, src, len, 0, 0);
    wchar_t* buf = new wchar_t[tmp_len+1];
    buf[tmp_len] = 0;
    ::MultiByteToWideChar(CP_UTF8, 0, src, len, buf, tmp_len+1);
    out = buf;
    delete[] buf;
}


std::string GetLocalPath(const std::string& name)
{
    wchar_t wpath[MAX_PATH];
    ::GetModuleFileNameW(NULL, wpath, MAX_PATH);
    
    std::string path;
    WideToUTF8(wpath, MAX_PATH, path);
    int s = path.length();
    path.replace(path.rfind("\\")+1, std::string::npos, name);
    return path;
}

bool RemoveLocalFile(const std::string& path)
{
    std::wstring wpath;
    UTF8ToWide(path.c_str(), path.length(), wpath);
    return ::DeleteFileW(wpath.c_str())?true:false;
}