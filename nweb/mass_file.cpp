#include <windows.h>
#include <assert.h>
#include <string>

#include "mass_file.h"
extern "C" unsigned long crc32( unsigned long crc,
                                const void *buf,
                                unsigned int len);
namespace nweb
{

//日记文件后缀
const char * MassFile::kJournalExt = ".ns";

MassFile::MassFile()
    : written_block_count_(0),
      total_block_count_(0)
{
    ;
}

MassFile::~MassFile()
{
    Close();
}

bool MassFile::Create(const char * file, uint64_t file_size)
{
    if(file_size > kMaxFileSize)
        return false;

    journal_path_ = file;
    journal_path_ += kJournalExt;

    if(!file_.Open(file, true))
        return false;

    if( !CreateJournal(journal_path_.data()) )
    {
        file_.Close();
        BlockFile::RemoveFile(file);
        return false;
    }

    if( !SetFileSize(file_size) )
    {
        Close();//删除日志
        BlockFile::RemoveFile(file);
        RemoveJournal();
        return false;
    }

    return true;  
}

bool MassFile::SetFileSize(uint64_t file_size)
{
    if(file_size > kMaxFileSize)
        return false;

    if(!file_.IsValid())
        return false;

    if(!file_.SetSize64(file_size))
        return false;

    //检查日志
    if(!CheckJournal())
    {//检查失败则重置日志
        if(!ResetJournal())
            return false;
    }
    UpdateDownloadedCount();
    return true;
}

uint64_t MassFile::GetFileSize() const
{
    return journal_.GetFileSize();
}

bool MassFile::Open(const char * file)
{
    //保存日志路径
    journal_path_ = file;
    journal_path_ += kJournalExt;

    if(!BlockFile::IsFileExist(file))
        return false;

    if( !file_.Open(file, false) ) 
        return false;//打开目标文件失败 

    if( !OpenJournal(journal_path_.data()) )
    { //日志文件打开失败 
        file_.Close();
        return false;
    }

    //验证目标文件大小
    uint64_t real_file_size = 0;   
    if(!file_.GetSize64(real_file_size))
        return false;

    if(real_file_size != journal_.GetFileSize())
    {//文件大小不相等
        Close();
        return false;
    }

    //验证日志有效性
    if(!CheckJournal())
    {//验证失败
        Close();
        return false;
    }
    //日志验证成功 或者日志验证失败但重置成功
    //从日志中获取已下载的块数
    UpdateDownloadedCount();
    return true;
}

void MassFile::Close()
{   
    CloseJournal();//关闭日志
    file_.Close();//关闭目标文件
}

bool MassFile::Finish()
{
    return RemoveJournal();
}

uint32_t MassFile::GetBlockCount() const
{
    return total_block_count_;
}

bool MassFile::IsBlockValid(uint32_t block_id) const
{
    uint32_t count  = GetBlockCount();
    if(count > block_id && journal_.IsValid())
    {
        return journal_.GetBlockStatus(block_id);
    }
    return false;
}

bool MassFile::SaveBlock(uint32_t block_id, const void * blob, size_t size)
{
    uint64_t block_start = 0;
    size_t block_size = 0;
    bool bret = false;
    if(GetBlockInfo(block_id, block_start, block_size))
    {//获取blockInfo成功
        if(block_size == size)
        {
            bret = file_.Write(blob, size, block_start);
            file_.Flush();
            //设置content文件修改时间
            file_.SetLastWriteTime();
            //更新日志
            UpdateJournal(block_id);
        }
    }
    return bret;
}

bool MassFile::GetBlockInfo(uint32_t block_id, 
                            uint64_t & start, size_t & size)const
{
    bool bret = false;
    uint64_t file_size = 0;
    if(journal_.IsValid())
    {
        file_size = journal_.GetFileSize();

        uint32_t count = GetBlockCount();
        if(count > 0 && block_id < count)
        {
            bret = true;
            uint64_t block_id64 = block_id;
            start = block_id64 * kMaxBlockSize;
            size = kMaxBlockSize;
            if(count - 1 == block_id && file_size % kMaxBlockSize)
            {
                size = file_size % kMaxBlockSize;
            }
        }
    }
    //assert(bret);
    return bret;
}

uint32_t MassFile::GetBlockId(uint64_t  start, size_t  size) const
{
    bool bret = false;
    uint32_t block_count = GetBlockCount();
    if(block_count > 0)
    {
        uint64_t every_start = 0;
        uint32_t every_size = 0;
        for(uint32_t i = 0; i < block_count; ++i)
        {
            if(GetBlockInfo(i, every_start, every_size))
                if(every_start == start && every_size == size)
                    return i;
        }
    }
    return kInvalidBlockId;
}

uint64_t MassFile::GetWrittenSize() const
{
    if(0 == written_block_count_)
        return 0;

    uint64_t written_size = 0; 
    uint32_t block_count = GetBlockCount();
    if(block_count > 0)
    {
        written_size = (written_block_count_ - 1) * kMaxBlockSize;
        if( IsBlockValid(block_count - 1) )
        {
            uint64_t start = 0;
            size_t size = 0;
            if( GetBlockInfo(block_count - 1, start, size) )
            {
                written_size += size;
            }
        }
        else
            written_size += kMaxBlockSize;
    }
    
    return written_size;
}

BlockQueue MassFile::FindInvalidBlocks() const
{
    BlockQueue invalid_blocks;
    uint32_t count = GetBlockCount();
    for(uint32_t i = 0; i < count; ++i)
    {
        if (!IsBlockValid(i))
            invalid_blocks.push(i);
    }
    return invalid_blocks;
}

bool MassFile::HasFinished() const
{
    return total_block_count_ == written_block_count_;
}

void MassFile::UpdateDownloadedCount()
{
    UpdateBlockCount();
    written_block_count_ = 0;
    uint32_t count = GetBlockCount();
    for(uint32_t i  = 0; i < count; ++i)
    {
        if(IsBlockValid(i))
        {
            written_block_count_ ++;
        }
    }
}

void MassFile::UpdateBlockCount()
{
    total_block_count_ = 0;
    uint64_t file_size = 0;
    if(file_.GetSize64(file_size) )
    {
        uint64_t count64 = file_size / kMaxBlockSize;
        if(file_size % kMaxBlockSize)
            count64++;

        if(count64 <= kMaxBlockCount)
            total_block_count_ = static_cast<uint32_t>(count64);
    }    
}

bool MassFile::RemoveJournal()
{
    bool bret = true;
    if(!journal_path_.empty())
    {
        CloseJournal();//关闭日志
        assert(!journal_path_.empty());
        bret = BlockFile::RemoveFile(journal_path_.data());
        journal_path_.clear();
    }
    return bret;
}

//创建日志文件
bool MassFile::CreateJournal(const char * file)
{
    if(journal_.IsValid())
    {//未清理上次使用的journal????
        assert(0);
        return true;
    }
    bool bret = journal_.Create(file);
    if(!bret)
        BlockFile::RemoveFile(file);

    return bret;
}
//打开日志文件
bool MassFile::OpenJournal(const char * file)
{
    return journal_.Open(file);
}

void MassFile::CloseJournal()
{
    journal_.Close();
    written_block_count_ = 0;
    total_block_count_ = 0;
}

bool MassFile::ResetJournal()
{
    bool bret = false;
    if(file_.IsValid() && journal_.IsValid())
    {
        journal_.Reset();
        uint64_t file_size = 0;
        //写入目标文件大小
        bret = file_.GetSize64(file_size);
        if(bret)
            journal_.UpdateFileSize(file_size);
        //重置后 未进行下载 写入CRC32和 last_modify无意义
    }
    return bret;
}

bool MassFile::CheckJournal()
{
    if(!file_.IsValid() || !journal_.IsValid())
        return false;

    if(!journal_.Validate())
        return false;

    int64_t file_last_modify = 0;
    if(!file_.GetLastWriteTime(file_last_modify))
        return false;

    if( journal_.GetLastModify() != file_last_modify )
        return false;

    //最近写入时间检验合格
    //检测文件大小
    uint64_t content_size = 0;
    if( !file_.GetSize64(content_size) )
        return false;

    if(journal_.GetFileSize() != content_size)
        return false;

    return true;
}

void MassFile::UpdateJournal(uint32_t block_id)
{//调用UpdateJournal时已经检测过block_id 这里不再检测block_id合法
    if(IsBlockValid(block_id))
    {
        assert(0);
        return;
    }
    journal_.UpdateBlockStatus(block_id, true);

    //设置写入时间
    int64_t file_time = 0;
    file_.GetLastWriteTime(file_time);
    journal_.UpdateLastModify(file_time);
    journal_.UpdateCrc();
    journal_.Flush();
    //已写入计数器+1
    written_block_count_++;
}

MassFile::Journal::Journal()
    :data_(0)
{
    ;
}

MassFile::Journal::~Journal()
{
    Close();
}

bool MassFile::Journal::Create(const char * journal_path)
{
    BlockFile file_journal;
    if(!file_journal.Open(journal_path, true))
        return false;

    //创建日志文件成功
    if(!file_journal.SetSize64(Journal::kExptectSize))
    {//设置日志大小失败 则关闭文件并删除
        file_journal.Close();
        BlockFile::RemoveFile(journal_path);
        return false;
    }

    //改变日志大小成功 进行文件映射
    if (void * p = BlockFile::OpenMapping(file_journal))
        data_ = reinterpret_cast<Data*>(p);

    file_journal.Close();
    return Reset();
}

bool MassFile::Journal::Open(const char * journal_path)
{
    //文件不存在 返回false
    if(!BlockFile::IsFileExist(journal_path))
        return false;

    //打开文件失败
    BlockFile file_journal;
    if(!file_journal.Open(journal_path, false))
        return false;

    uint64_t journal_size;
    if (!file_journal.GetSize64(journal_size))
    {
        file_journal.Close();
        return false;
    }

    if( Journal::kExptectSize != journal_size)
    {
        file_journal.Close();
        return false;
    }
    //日志文件大小验证合格 使用FileMaping打开文件
    void * p = BlockFile::OpenMapping(file_journal);
    if(!p)
    {
        file_journal.Close();
        return false;
    }

    data_ = reinterpret_cast<Data *>(p);
    //关闭文件句柄
    file_journal.Close();

    if(!Validate())
    {
        BlockFile::CloseMapping(data_);
        data_ = 0;
        return false;
    }
    return true;
}

void MassFile::Journal::Close()
{
    if(data_)
    {        
        BlockFile::CloseMapping(data_);
        data_ = 0;
    }
}


bool MassFile::Journal::Reset()
{
    if(!data_)
        return false;
    memset(data_, 0, sizeof(Journal));
    //写入标志
    data_->head.magic = Journal::kMagic;
    return true;
}

bool MassFile::Journal::Validate()
{
    if(!data_)
        return false;

    if(data_->head.magic != Journal::kMagic)
        return false;

    //标志合格
    uint32_t hash = crc32(0xffffffff, &data_->body, sizeof(data_->body));
    if(hash == data_->head.crc32)
    {//CRC32验证通过
        return true;
    }
    return false;
}

bool MassFile::Journal::IsValid() const
{
    return data_ != 0;
}

void MassFile::Journal::Flush()
{
    if(data_)
        BlockFile::FlushMapping(data_, Journal::kExptectSize);
}

int64_t MassFile::Journal::GetLastModify() const
{
    if(data_)
        return data_->body.last_modify;
    return -1;
}

uint64_t MassFile::Journal::GetFileSize() const
{
    if(data_)
        return data_->body.file_size;
    return -1;
}

bool MassFile::Journal::GetBlockStatus(uint32_t block_id) const
{
    if(!data_)
        return false;

    int index = block_id / 8;
    int bit_index = block_id % 8;
    return ( data_->body.block_status[index] >> bit_index ) & 0x1;
}

void MassFile::Journal::UpdateFileSize(uint64_t file_size)
{
    if(data_)
        data_->body.file_size = file_size;
}

void MassFile::Journal::UpdateLastModify(int64_t last_modify)
{
    if(data_)
        data_->body.last_modify = last_modify;
}

void MassFile::Journal::UpdateBlockStatus(uint32_t block_id, bool valid)
{
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    uint8_t value = valid ? 1 : 0;
    if(data_)
        data_->body.block_status[byte_index] |= (value << bit_index);
}

void MassFile::Journal::UpdateCrc()
{
    if(!data_)
        return;
    data_->head.crc32 = crc32(0xffffffff, &data_->body, sizeof(data_->body));
}

}