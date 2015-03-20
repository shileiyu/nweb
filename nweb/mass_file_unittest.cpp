#include "nweb_test.h"
#include "mass_file.h"

#pragma execution_character_set("utf-8")

namespace 
{
namespace utils
{

//删除文件函数
void RemoveFile(const char * file)
{
    wchar_t name16[MAX_PATH];    
    ::MultiByteToWideChar(CP_UTF8, 0, file, -1,
        name16, MAX_PATH);
    DeleteFileW(name16);
}

bool IsFileExist(const char * name)
{//文件是否存在
    wchar_t name16[MAX_PATH];    
    if(!::MultiByteToWideChar(CP_UTF8, 0, name, -1, name16, MAX_PATH))
        return false;

    DWORD attr = 0;
    attr = GetFileAttributesW(name16);
    if(!(attr & FILE_ATTRIBUTE_DIRECTORY) && 
        attr != INVALID_FILE_ATTRIBUTES)
    {
        return true;
    }

    return false;
}


}

TEST(MassFileTest, CreateFileTest)
{
    nweb::MassFile mass_file;

    const char * content_path = ".\\build\\test\\我.txt";
    const char * journal_path = ".\\build\\test\\\xe6\x88\x91.txt.ns";

    //确保2个文件不存在
    utils::RemoveFile(content_path);
    utils::RemoveFile(journal_path);
    ASSERT_FALSE(utils::IsFileExist(content_path));
    ASSERT_FALSE(utils::IsFileExist(journal_path));
    //2个文件都不存在的情况下 Open会创建2个文件
    ASSERT_TRUE(mass_file.Create(content_path, 1024 * 1024));
    //确保文件成功创建
    ASSERT_TRUE(utils::IsFileExist(content_path));
    ASSERT_TRUE(utils::IsFileExist(journal_path));

    mass_file.Close();
    //2个文件都存在的情况下 未写入时  Valadate通不过
    ASSERT_FALSE(mass_file.Open(content_path));
    mass_file.Close();
    utils::RemoveFile(content_path);
    ASSERT_TRUE(mass_file.Finish());
}

TEST(MassFileTest, OpenFileTest)
{
    nweb::MassFile mass_file;

    const char * content_path = ".\\build\\test\\\xe6\x88\x91.txt";
    const char * journal_path = ".\\build\\test\\\xe6\x88\x91.txt.ns";
    //文件已经成功创建
    ASSERT_FALSE(utils::IsFileExist(content_path));
    ASSERT_FALSE(utils::IsFileExist(journal_path));
    ASSERT_TRUE(mass_file.Create(content_path, 1024 * 1024));
    ASSERT_TRUE(utils::IsFileExist(content_path));
    ASSERT_TRUE(utils::IsFileExist(journal_path));
    mass_file.Close();

    //仅保留1个文件时 能否正确创建
    //移除目标文件
    utils::RemoveFile(content_path);
    ASSERT_FALSE(utils::IsFileExist(content_path));
    ASSERT_FALSE(mass_file.Open(content_path));
    mass_file.Close();

    //移除日志文件时
    utils::RemoveFile(journal_path);
    ASSERT_FALSE(utils::IsFileExist(journal_path));
    ASSERT_FALSE(mass_file.Open(content_path));

    mass_file.Close();
    utils::RemoveFile(content_path);
    ASSERT_TRUE(mass_file.Finish());
}
//测试公共接口
TEST(MassFileTest, PublicFileTest)
{
    nweb::MassFile mass_file;

    const char * content1_path = ".\\build\\test\\\xe6\x88\x91m.txt";
    const char * journal1_path = ".\\build\\test\\\xe6\x88\x91m.txt.ns";
    //确保2个文件不存在
    utils::RemoveFile(content1_path);
    utils::RemoveFile(journal1_path);
    ASSERT_FALSE(utils::IsFileExist(content1_path));
    ASSERT_FALSE(utils::IsFileExist(journal1_path));
    //2个文件都不存在的情况下 Open会创建2个文件
    ASSERT_TRUE(mass_file.Create(content1_path, 1024 * 1024));

    ASSERT_EQ(1, mass_file.GetBlockCount());
    ASSERT_FALSE(mass_file.IsBlockValid(0));

    uint64_t start;
    size_t size;
    mass_file.GetBlockInfo(0, start, size);
    ASSERT_EQ(start, 0);
    ASSERT_EQ(size, 1024 * 1024);
    ASSERT_FALSE(mass_file.GetBlockInfo(2, start, size));

    uint32_t block_id;
    block_id = mass_file.GetBlockId(start, size);
    ASSERT_EQ(block_id, 0);
    block_id = mass_file.GetBlockId(0, 4 * 1024 * 1024);
    ASSERT_EQ(block_id, nweb::MassFile::kInvalidBlockId);
    //
    mass_file.Close();
    utils::RemoveFile(content1_path);
    ASSERT_TRUE(mass_file.Finish());
    ASSERT_FALSE(utils::IsFileExist(journal1_path));

    const char * content2_path = ".\\build\\test\\\xe6\x88\x91n.txt";
    const char * journal2_path = ".\\build\\test\\\xe6\x88\x91n.txt.ns";

    //确保2个文件不存在
    utils::RemoveFile(content2_path);
    utils::RemoveFile(journal2_path);
    ASSERT_FALSE(utils::IsFileExist(content2_path));
    ASSERT_FALSE(utils::IsFileExist(journal2_path));
    //2个文件都不存在的情况下 Open会创建2个文件 101m目标文件
    ASSERT_TRUE(mass_file.Create(content2_path, 101 * 1024 * 1024));

    //101M 文件分为26块
    ASSERT_EQ(26, mass_file.GetBlockCount());
    ASSERT_FALSE(mass_file.IsBlockValid(1));
    ASSERT_FALSE(mass_file.IsBlockValid(2));
    ASSERT_FALSE(mass_file.IsBlockValid(0));
    //第1个分块是4M
    mass_file.GetBlockInfo(0, start, size);
    ASSERT_EQ(start, 0);
    ASSERT_EQ(size, 4 * 1024 * 1024);
    //第2个分块是4M
    mass_file.GetBlockInfo(1, start, size);
    ASSERT_EQ(start, 4 * 1024 * 1024);
    ASSERT_EQ(size, 4 * 1024 * 1024);
    //第26个分块是1M
    mass_file.GetBlockInfo(25, start, size);
    ASSERT_EQ(start, 4 * 1024 * 1024 * 25);
    ASSERT_EQ(size, 1024 * 1024);
    //测试写入数据
    char * mmm = new char[4 * 1024 * 1024];
    memset(mmm, 1, (4 * 1024 * 1024));
    ASSERT_TRUE(mass_file.SaveBlock(0, mmm, 4 * 1024 * 1024));
    ASSERT_TRUE(mass_file.IsBlockValid(0));

    ASSERT_TRUE(mass_file.SaveBlock(23, mmm, 4 * 1024 * 1024));
    ASSERT_TRUE(mass_file.IsBlockValid(23));

    ASSERT_TRUE(mass_file.SaveBlock(25, mmm, 1024 * 1024));
    ASSERT_TRUE(mass_file.IsBlockValid(25));
    ASSERT_FALSE(mass_file.SaveBlock(26, mmm, 1024 * 1024));
    ::Sleep(/*60* */1000);
    mass_file.Close();


    ASSERT_TRUE(mass_file.Open(content2_path));
    for(uint32_t i = 0; i < mass_file.GetBlockCount(); ++i)
    {
        if(i == 23 || i == 25 || i == 0)
            ASSERT_TRUE(mass_file.IsBlockValid(i));
        else
            ASSERT_FALSE(mass_file.IsBlockValid(i));
    }
    
    mass_file.Close();
    utils::RemoveFile(content2_path);
    ASSERT_TRUE(mass_file.Finish());
    delete [] mmm;
}


//测试Journal FLUSH后 是否立刻写入到磁盘

}
