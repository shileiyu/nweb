#include "block_file.h"


namespace nweb
{
//static member functions
int UTF8Decode(const char * multibyte,	const int multibyte_size,	
               wchar_t * widechar, int widechar_size)
{
    int written = ::MultiByteToWideChar(CP_UTF8, 0, multibyte, multibyte_size,
                                        widechar, widechar_size);
    if(written > 0 && written < widechar_size)
        widechar[written] = 0;
    return written;
}

void FileTimeToUnixTime(const LPFILETIME ft, time_t & t)
{
    ULARGE_INTEGER ui;
    ui.LowPart = ft->dwLowDateTime;
    ui.HighPart = ft->dwHighDateTime;
    t = ((ui.QuadPart - 116444736000000000) / 10000000);
}

void UnixTimeToFileTime( time_t t, LPFILETIME pft )
{
    ULARGE_INTEGER ui;
    ui.QuadPart = Int32x32To64(t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = ui.LowPart;
    pft->dwHighDateTime = ui.HighPart;
}

std::string BlockFile::GetPathFromFullName(const char * fullname)
{
    std::string path;
    if(!fullname)
        return path;

    auto last_backslash = std::strrchr(fullname, '\\');
    auto last_slash = std::strrchr(fullname, '/');
    auto slash = (std::max)(last_backslash, last_slash);

    if(!slash)
        return path;

    return path.assign(fullname, slash - fullname);
}

bool BlockFile::CreateDirectoryRecursive(const char * name)
{
    bool result = false;
    DWORD attr = 0;

    wchar_t name16[MAX_PATH] = {0};    
    if(!UTF8Decode(name, -1, name16, MAX_PATH))
        return false;

    attr = ::GetFileAttributes(name16);

    if(attr != INVALID_FILE_ATTRIBUTES)
        return attr & FILE_ATTRIBUTE_DIRECTORY ? true : false;

    wchar_t partical[MAX_PATH] = {0};
    for(size_t i = 0; i < MAX_PATH; ++i)
    {
        wchar_t ch = name16[i];

        if(ch == L'/' || ch == L'\\' || ch == 0)
        {
            attr = ::GetFileAttributes(partical);
            if(attr == INVALID_FILE_ATTRIBUTES)
                result = ::CreateDirectory(partical, 0) != FALSE;
            else
                result = attr & FILE_ATTRIBUTE_DIRECTORY ? true : false;

            if(!result || ch == 0)
                break;
            else
                ch = '/';
        }

        if(!(partical[i] = ch))
            break;
    }

    return result;
}

bool BlockFile::RemoveFile(const char * file)
{
    if(IsFileExist(file))
    {//文件存在
        wchar_t name16[MAX_PATH] = {0};    
        if(!UTF8Decode(file, -1, name16, MAX_PATH))
            return false;
        return DeleteFile(name16) ? true : false;
    }
    return true;
}

void BlockFile::CloseMapping(void * file_mapping_data)
{
    if (file_mapping_data) 
        ::UnmapViewOfFile(file_mapping_data);
    return;
}

bool BlockFile::FlushMapping( void* file_mapping_data, int32_t size )
{
    if (!file_mapping_data)
        return false;

    return ::FlushViewOfFile(file_mapping_data, size) != FALSE;
}

//nonstatic member funtions
BlockFile::BlockFile()
{
    handle_ = INVALID_HANDLE_VALUE;
}

BlockFile::~BlockFile()
{
    Close();
}


void BlockFile::Close()
{
    if (INVALID_HANDLE_VALUE != handle_)
    {
        ::CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    return ;
}

bool BlockFile::Open(const char * file, bool bcreate)
{
    wchar_t name16[MAX_PATH] = {0};    
    if(!UTF8Decode(file, -1, name16, MAX_PATH))
        return false;
    DWORD dwCreationDisposition = CREATE_ALWAYS;
    if(false == bcreate)
        dwCreationDisposition = OPEN_EXISTING;
    else
    {//目录不存在 则创建目录
        auto path = GetPathFromFullName(file);
        if(!path.empty())
            CreateDirectoryRecursive(path.data());
    }

    handle_ = ::CreateFile(name16, GENERIC_READ | GENERIC_WRITE, 
                           FILE_SHARE_READ, 0, dwCreationDisposition,
                           FILE_FLAG_WRITE_THROUGH, 0);

    return INVALID_HANDLE_VALUE != handle_;
}

bool BlockFile::Write(const void * data, uint32_t size_to_write, uint64_t offset)
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    if(data == 0)
        return false;
    
    DWORD transfered = 0;
    OVERLAPPED status;
    const char * blob = reinterpret_cast<const char *>(data);
    while(size_to_write)
    {
        memset(&status, 0 , sizeof(status));
        status.Offset = static_cast<uint32_t>(offset);
        status.OffsetHigh = static_cast<uint32_t>(offset >> 32);

        if(!::WriteFile(handle_, blob, size_to_write, &transfered, &status))
            return false;
        size_to_write -= transfered;
        offset += transfered;
        blob += transfered; 
    }
    return true;
}

bool BlockFile::Write(const void * data, uint32_t size_to_write)
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    if(data == 0)
        return false;
    
    DWORD transfered = 0;
    const char * blob = reinterpret_cast<const char *>(data);
    while(size_to_write)
    {

        if(!::WriteFile(handle_, blob, size_to_write, &transfered, 0))
            return false;
        size_to_write -= transfered;
        blob += transfered; 
    }
    return true;
}

bool BlockFile::IsFileExist(const char * name)
{//文件是否存在
    wchar_t name16[MAX_PATH] = {0};    
    if(!UTF8Decode(name, -1, name16, MAX_PATH))
        return false;

    DWORD attr = 0;
    attr = GetFileAttributes(name16);
    if(!(attr & FILE_ATTRIBUTE_DIRECTORY) && 
        attr != INVALID_FILE_ATTRIBUTES)
    {
        return true;
    }

    return false;
}

bool BlockFile::SetSize64(uint64_t file_size)
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER pos = {0};
    LARGE_INTEGER newpos = {0};
    pos.QuadPart = file_size;
    if(!SetFilePointerEx(handle_, pos, &newpos, FILE_BEGIN))
        return false;

    return SetEndOfFile(handle_) != FALSE;
}    

bool BlockFile::GetSize64(uint64_t & file_size) const
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER  size = {0};
    if(::GetFileSizeEx(handle_, &size) != 0)
    {
        file_size = size.QuadPart;
        return true;
    }
    return  false;
}

bool BlockFile::Flush()
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    return ::FlushFileBuffers(handle_) != FALSE;
}

bool BlockFile::SetLastWriteTime()
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    FILETIME ft;
    ::GetSystemTimeAsFileTime(&ft);
    return ::SetFileTime(handle_, 0, 0, &ft) != FALSE;
}

bool BlockFile::SetLastWriteTime(time_t time)
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    FILETIME ft;
    UnixTimeToFileTime(time, &ft);
    return ::SetFileTime(handle_, 0, 0, &ft) != FALSE;
}


bool BlockFile::GetLastWriteTime( time_t & last_modify )
{
    if(handle_ == INVALID_HANDLE_VALUE)
        return false;

    FILETIME ft = {0};
    BOOL bret = ::GetFileTime(handle_, 0, 0, &ft);
    if(bret)
    {
        FileTimeToUnixTime(&ft, last_modify);
    }

    return bret ? true : false;
}

bool BlockFile::IsValid() const
{
    return handle_ != INVALID_HANDLE_VALUE;
}

void * BlockFile::OpenMapping(BlockFile & file)
{
    void * mapping = nullptr;

    if(file.handle_ == INVALID_HANDLE_VALUE)
        return nullptr;

    HANDLE fm = ::CreateFileMapping(file.handle_, 0, PAGE_READWRITE, 0, 0, 0);
    if(INVALID_HANDLE_VALUE != fm)
    {//成功创建
         mapping = ::MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        ::CloseHandle(fm);
    }
    return mapping;
}

bool BlockFile::Truncate()
{
    if(handle_ != INVALID_HANDLE_VALUE)
        return SetEndOfFile(handle_) != FALSE;
    return false;
}

}