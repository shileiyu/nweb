#ifndef NWEB_MASS_FILE_H_
#define NWEB_MASS_FILE_H_

#include <queue>
#include "block_file.h"


namespace nweb
{

typedef std::queue<uint32_t> BlockQueue;

class MassFile
{
private:
    //日志文件的结构
    class Journal
    {
    public:
        struct Data
        {
            struct
            {
                uint32_t magic;
                uint32_t crc32;
            } head;

            struct
            {
                uint64_t file_size;
                int64_t last_modify;
                uint64_t reserve[125];
                uint8_t block_status[3072];
            } body;
        };
    public:
        Journal();

        ~Journal();

        bool Create(const char * journal_path);
        //打开日志
        bool Open(const char * journal_path);
        //关闭日志
        void Close();
        //重置日志
        bool Reset();
        //检查日志
        bool Validate();

        bool IsValid() const;

        int64_t GetLastModify() const;

        uint64_t GetFileSize() const ;

        bool GetBlockStatus(uint32_t block_id)const ;

        void UpdateFileSize(uint64_t file_size);
        
        void UpdateLastModify(int64_t last_modify);

        void UpdateBlockStatus(uint32_t block_id, bool valid);

        void UpdateCrc();

        void Flush();
    private:
        Data * data_;
    private:
        static const uint32_t kMagic = 0x30304e53;//NS00
        static const uint32_t kExptectSize = 4096;
    };

public:
    static const uint32_t kInvalidBlockId = -1;
public:
    MassFile();

    ~MassFile();

    bool Create(const char * file, uint64_t file_size);

    bool SetFileSize(uint64_t file_size);

    uint64_t GetFileSize() const;

    bool Open(const char * file);

    void Close();

    bool Finish();//下载完成后删除日志文件
    //block_id [0, count)
    uint32_t GetBlockCount() const;
    bool IsBlockValid(uint32_t block_id) const;
    bool SaveBlock(uint32_t block_id, const void * blob, size_t size);
    bool GetBlockInfo(uint32_t block_id, uint64_t & start, size_t & size) const;
    uint32_t GetBlockId(uint64_t start, size_t size) const;
    //!返回已经写入的数据大小
    uint64_t GetWrittenSize() const;
    
    BlockQueue FindInvalidBlocks() const;

    bool HasFinished() const;
private:
    //设置文件的 最近写入时间
    bool SetLastWriteTime();
    //获取文件的 最近写入时间
    bool GetLastWriteTime(int64_t & last_modify);
private:
    bool RemoveJournal();
    bool OpenJournal(const char * file);
    void CloseJournal();
    bool CreateJournal(const char * file);
    void UpdateJournal(uint32_t block_id);
    //!内存映射打开日志文件
    bool OpenJournalMapping(void * journal_content);
    bool ResetJournal();
    bool CheckJournal();
    uint32_t CalcCrcJournal();

    void UpdateDownloadedCount();

    void UpdateBlockCount();
private:
    BlockFile file_;

    Journal journal_;
    std::string journal_path_;
    uint32_t written_block_count_;
    uint32_t total_block_count_;
private:
    static const uint32_t kMaxBlockCount = 0x1800;
    static const uint32_t kMaxBlockSize = 0x400000;
    static const uint64_t kMaxFileSize = 1ll * kMaxBlockCount * kMaxBlockSize;
    static const char * kJournalExt;
};


}

#endif