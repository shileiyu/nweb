#include <curl/curl.h>
#include "nweb.h"
#include "http.h"
#include "http_foreman.h"

namespace nweb
{

//retry times of fetching http content when failed
const uint32_t kMaxHttpRetryTimes = 256;

template<typename T>
static inline size_t countof(const T & arr)
{
    return sizeof(arr)/sizeof(arr[0]);
}

static uint32_t Tick()
{
    return GetTickCount() / 1000;
}

class Block : public HttpResponse 
{
private:
    void * data_;
    size_t size_;
    size_t capacity_;

public:
    Block() : size_(0), data_(0), capacity_() {}

    virtual ~Block()
    {
        if (data_) 
        {
            free(data_);
            data_ = nullptr;
        }
        size_ = 0;
        capacity_ = 0;
    }
        
    size_t WriteChunk(const void * blob, size_t size)
    {
        
        if(size > capacity_ - size_)
            if(!Expend(size_ + size))
                return 0;

        void * dst = reinterpret_cast<char *>(data_) + size_;
        memcpy(dst, blob, size);
        size_ += size;
        return size;
    }

    bool Expend(size_t size)
    {
        const size_t kStep = 0x400000;
        size_t new_size = size / kStep * kStep + kStep;
        data_ = realloc(data_, new_size);
        if(!data_)
            return false;
        capacity_ = new_size;
        return true;
    }

    void Reset()
    {
        size_ = 0;
        headers_.clear();
    }

    size_t size() const
    {
        return size_;
    }

    const void * data() const
    {
        return data_;
    }
};


class HttpChannel
{
public:
    enum Error
    {
        kIdle,
        kDone,
        kAgain,
        kFailed,
    };

private:
    HttpConnection conn_;
    HttpRequest  request_;
    Block block_;
    bool has_open_;

public:
    HttpChannel() : has_open_(false) {}

    ~HttpChannel() 
    {
        conn_.fini();
    }
        
    bool Open(const char * url, bool nobody)
    {
        if(has_open_)
            return false;

        conn_.SetUrl(url);
        if(nobody)
            conn_.SetRequestMethod(HttpRequestMethod::kHead);
        else 
            conn_.SetRequestMethod(HttpRequestMethod::kGet);
        conn_.SetRequest(&request_);
        conn_.SetResponse(&block_);
        conn_.SetLowSpeedLimit(8, 60);
        conn_.SetConnectTimeout(60000);
        conn_.EnableRedirection(true);
        conn_.SetMaxRedirection(5);
        has_open_ = true;
        return true;
    }

    void Close()
    {
        conn_.Reset();
        block_.Reset();
        has_open_ = false;
    }

    Error Transfer(uint32_t & in)
    {
        in = 0;

        if(!conn_.LazyInitialize())
            return kFailed;
        
        if(!has_open_)
            return kIdle;

        auto result = conn_.AsyncPerform();
        in = conn_.InSize();
        if(result == kConnOK)
            return kDone;
        else if(result != kConnAgain)
            return kFailed;
        conn_.Wait(5);
        return kAgain;
    }

    const Block & block() const
    {
        return block_;
    }

    void SetRange(const HttpRange & range)
    {
        if(range.size())
            request_.SetRange(range.first(), range.last());
    };

    void RemoveRange()
    {
        request_.RemoveHeader("Range");
    };

    const char * EffectiveURL() const
    {
        return conn_.GetUrl();
    }
};

/*
HttpForeman
*/
HttpForeman::HttpForeman()
    : stage_(kFetchStagePrepare),
      retry_count_(0), 
      expected_length_(-1)
{
    memset(channels_, 0, sizeof(channels_));
}

HttpForeman::~HttpForeman()
{
    DestroyChannels();
}

void HttpForeman::SetPrimaryUrl( const char* url )
{
    url_ = url;
}

void HttpForeman::SetFilePath( const char* path )
{
    path_ = path;
}

void HttpForeman::SetFileSize( uint64_t filesize )
{
    expected_length_ = filesize;
}

Result HttpForeman::Fetch()
{
    input_stats_ = 0;
    switch(stage_)
    {
    case kFetchStagePrepare:
        return DoPrepare();
    case kFetchStageScout:    
        return DoScout();
    case kFetchStageDownload:
        return DoDownload();
    }
    return kResultFailed;
}


uint64_t HttpForeman::FetchedSize() const
{
    //downloaded size
    uint64_t result = mass_file_.GetWrittenSize();
    //downloading size
    if(HasFinished())
        return result;
    
    result += DownloadingSize();
    return result;
}

uint64_t HttpForeman::TotalSize() const
{
    return expected_length_ == -1 ? 0 : expected_length_;
}

uint32_t HttpForeman::InputStats() const
{
    return input_stats_;
}

Result HttpForeman::DoPrepare()
{
    if(url_.empty())
        return kResultFailed;

    if(path_.empty())
        return kResultFailed;

    if(!CreateChannels())
        return kResultFailed;

    retry_count_ = 0;
    stage_ = kFetchStageScout;
    return kResultAgain;
}

Result HttpForeman::DoScout()
{
    uint32_t in = 0;
    auto scout = channels_[0];
    if(!scout)
        return kResultFailed;

    auto error = scout->Transfer(in);
    switch(error)
    {
    case HttpChannel::kIdle:
        {
            scout->RemoveRange();
            scout->Open(url_.data(), true);
            return kResultAgain;
        }
    case HttpChannel::kDone:
        {
            auto & block = scout->block();
            if(block.GetStatusCode() != HttpStatusCode::kOK)
                return kResultFailed;
            if(expected_length_ != -1)
                if(block.GetContentLength() != expected_length_)
                    return kResultFileSizeMismatch;
            if(!block.HasContentLength())
                return kResultFileSizeUnknown;
            expected_length_ = block.GetContentLength();
            //open mass file
            const char * filename = path_.data();
            bool resumeable = false;
            if(mass_file_.Open(filename))
                if(mass_file_.GetFileSize() == expected_length_)
                    resumeable = true;
            if(!resumeable)
                if(!mass_file_.Create(filename, expected_length_))
                    return kResultOpenFileFailded;

            pendding_blocks_ = mass_file_.FindInvalidBlocks();
            //更新URL
            url_ = scout->EffectiveURL();
            scout->Close();
            retry_count_ = 0;    
            stage_ = kFetchStageDownload;
            return kResultAgain;
        }
    case HttpChannel::kFailed:
        {
            if(retry_count_++ > kMaxHttpRetryTimes)
                return kResultFailed;
            scout->Close();
            scout->Open(url_.data(), true);
            return kResultAgain;
        }
    case HttpChannel::kAgain:
        return kResultAgain;
    }
    return kResultFailed;
}

Result HttpForeman::DoDownload()
{
    if(HasFinished()) 
    {
        mass_file_.Finish();
        mass_file_.Close();
        return kResultOK;
    }

    for(size_t i = 0; i < countof(channels_); ++i)
    {
        auto worker = channels_[i];
        if(!worker)
            return kResultFailed;

        uint32_t in = 0;
        auto error = worker->Transfer(in);
        input_stats_ += in;
        switch(error)
        {
        case HttpChannel::kIdle: 
            {
                if(pendding_blocks_.empty())
                    break;
                uint32_t bid = pendding_blocks_.front();
                pendding_blocks_.pop();
                uint64_t offset = 0;
                size_t size = 0;;
                mass_file_.GetBlockInfo(bid, offset, size);
                HttpRange range(offset, size);
                worker->SetRange(range);
                worker->Open(url_.data(), false);
                break;
            }
        case HttpChannel::kDone: 
            {
                const Block & block = worker->block();
                auto range = block.GetContentRange();
                auto bid = mass_file_.GetBlockId(range.first(), range.size());
                auto data = block.data();
                auto size = block.size();
                if(!mass_file_.SaveBlock(bid, data, size)) 
                    return kResultSaveBlockFailded;
                worker->Close();
                retry_count_ = 0;
                break;
            }
        case HttpChannel::kFailed:
            {
                if(retry_count_++ > kMaxHttpRetryTimes)
                    return kResultFailed;
                worker->Close();
                worker->Open(url_.data(), false);
                break;
            }
        case HttpChannel::kAgain:
            break;
        }
    }

    return kResultAgain;
}

bool HttpForeman::HasFinished() const
{
    return mass_file_.HasFinished();
}

void HttpForeman::Reset()
{
    CloseChannels();
    mass_file_.Close();
    retry_count_ = 0;
    url_.clear();
    path_.clear();
    expected_length_ = -1;
    stage_ = kFetchStagePrepare;
    pendding_blocks_.swap(BlockQueue());
}

bool HttpForeman::CreateChannels()
{
    for(size_t i = 0; i < countof(channels_); ++i)
    {
        if(!channels_[i])
            channels_[i] = new HttpChannel();
        if(!channels_[i])
            return false;
    }
    return true;
}

void HttpForeman::DestroyChannels()
{
    for(size_t i = 0; i < countof(channels_); ++i)
    {
        if(channels_[i])
        {
            delete channels_[i];
            channels_[i] = nullptr;
        }
    }
}

void HttpForeman::CloseChannels()
{
    for(size_t i = 0; i < countof(channels_); ++i)
    {
        HttpChannel * channel = channels_[i];
        if(channel)
            channel->Close();
    }
}

uint64_t HttpForeman::DownloadingSize() const
{
    uint64_t result = 0;
    for(size_t i = 0; i < countof(channels_); ++i)
    {
        auto channel = channels_[i];
        if(channel)
            result += channel->block().size();
    }
    return result;
}

}