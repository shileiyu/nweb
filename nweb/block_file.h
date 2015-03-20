#ifndef NWEB_BLOCK_FILE_H_
#define NWEB_BLOCK_FILE_H_

#include "nweb.h"

namespace nweb
{

class BlockFile
{
public:
    BlockFile();
    ~BlockFile();

    static bool IsFileExist(const char * name);

    static bool RemoveFile(const char * file);

    static std::string GetPathFromFullName(const char * fullname);

    static bool FlushMapping(void* file_mapping_data, int32_t size);

    static bool CreateDirectoryRecursive(const char * name);

    static void * OpenMapping(BlockFile & file);

    static void CloseMapping(void * mapping);

    bool Open(const char * file, bool bcreate);

    void Close();

    bool Write(const void * data, uint32_t size_to_write, uint64_t offset);

    bool Write(const void * data, uint32_t size_to_write);

    bool Flush();

    bool SetSize64(uint64_t file_size);

    bool GetSize64(uint64_t& file_size) const;

    bool SetLastWriteTime();

    bool SetLastWriteTime(time_t time);

    bool GetLastWriteTime(time_t & time);

    bool IsValid() const;

    bool Truncate();
private:
    /* data */
    HANDLE handle_;
};

    
}
    
#endif