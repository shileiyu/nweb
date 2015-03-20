#ifndef NWEB_HTTP_FOREMAN_H_
#define NWEB_HTTP_FOREMAN_H_

#include <stdint.h>
#include <string>
#include <queue>
#include <algorithm>
#include "mass_file.h"
#include "speed_meter.h"

namespace nweb
{

class HttpChannel;

class HttpForeman
{
private:
    enum FetchStage
    {
        kFetchStagePrepare,
        kFetchStageScout,
        kFetchStageDownload,
    };

public:
    HttpForeman();
    ~HttpForeman();
     
    /*
    * 下载文件接口
    */
    void SetPrimaryUrl(const char* url);
    void SetFilePath(const char* path);
    void SetFileSize(uint64_t filesize);
    //异步下载接口
    Result Fetch();
    //重置
    void Reset();

    /* 已下载容量 */
    uint64_t FetchedSize() const;
    /* 下载目标总容量 */
    uint64_t TotalSize() const;

    uint32_t InputStats() const;

private:
    Result DoPrepare();

    Result DoScout();
    
    Result DoDownload();

    bool HasFinished() const;

    bool CreateChannels();

    void DestroyChannels();

    void CloseChannels();

    uint64_t DownloadingSize() const;
private:
    uint32_t retry_count_;
    std::string url_;
    std::string path_;
    FetchStage stage_;
    uint64_t expected_length_;
    BlockQueue pendding_blocks_;
    MassFile mass_file_;
    HttpChannel * channels_[1];
    uint32_t input_stats_;
};

}

#endif
