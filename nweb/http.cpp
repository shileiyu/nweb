#include <curl\curl.h>
#include <curl\curl_ext.h>
#include "url.h"
#include "http.h"


namespace nweb
{

const char * kContentLength     = "content-length";
const char * kLastModified      = "last-modified";
const char * kContentRange      = "content-range";

const char * strnchr(const char * str, size_t len, char chr) 
{
    for(const char * end = str + len; str < end; ++str)
    {
        if (*str == chr) 
            return str;
    }
    return nullptr;
}

uint64_t strtoui64(const char * str)
{
    char ** end = 0;
    return _strtoui64(str, end, 10);
}

int parse_status_code(const char * line, size_t length)
{
    const char * kPattern = "HTTP/1.1";

    for(size_t i = 0; i < length; ++i)
    {
        if(kPattern[i] == 0)
        {
            if(length - i > 4)
            {
                char code[8] = {0};
                strncpy_s(code, &line[i], 4);
                return std::atoi(code);
            }
        }

        if(line[i] != kPattern[i])
            break;
    }
    return 0;
}

typedef std::pair<std::string, std::string> Header;

Header parse_header(const char * line, size_t length)
{
    Header header;
    
    if(!length)
        return header;
    auto colon = strnchr(line, length, ':');
    if(!colon)
        return header;

    size_t key_len = colon - line;
    const char * value = colon + 1;
    size_t value_len = length - key_len - 1;

    //trim left whitespace in value
    while(value_len)
    {
        if(*value == ' ') 
        {
            ++value;
            --value_len;
            continue;
        }
        break;
    }
    header.first.assign(line, key_len);
    header.second.assign(value, value_len);
    /* store key in low case */
    std::string & key = header.first;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    return header;
}

HttpConnResult TranslateCurlCode(CURLcode code)
{
    switch(code)
    {
    case CURLE_OK:
        return kConnOK;
    case CURLE_PARTIAL_FILE:
        return kConnPartialDone;
    case CURLE_OPERATION_TIMEDOUT:
        return kConnTimeout;
    }
    return kConnFail;
}

HttpRange::HttpRange()
{
    offset_ = 0;
    size_ = 0;
}

HttpRange::HttpRange(uint64_t offset, size_t size)
{
    offset_ = offset;
    size_ = size;
}

HttpRange::HttpRange(const HttpRange & obj)
{
    offset_ = obj.offset_;
    size_ = obj.size_;
}

HttpRange & HttpRange::operator=(const HttpRange & obj)
{
    offset_ = obj.offset_;
    size_ = obj.size_;
    return *this;
}

uint64_t HttpRange::offset() const
{
    return offset_;
}

uint64_t HttpRange::first() const
{
    return offset_;
}

uint64_t HttpRange::last() const
{
    return offset_ + size_ - 1;
}

size_t HttpRange::size() const
{
    return size_;
}

//HttpRequest
void HttpRequest::SetRange(uint64_t first, uint64_t last)
{
    char litery_range[64];
    sprintf_s(litery_range, "bytes=%I64u-%I64u", first, last);
    AddHeader("Range", litery_range);
}

void HttpRequest::SetRange(const HttpRange & range)
{
    return SetRange(range.first(), range.last()); 
}

void HttpRequest::AddHeader(const char * key, const char * value)
{
    headers_[key] = value;
}

void HttpRequest::RemoveHeader(const char * key)
{
    auto iter = headers_.find(key);
    if(iter != headers_.end())
        headers_.erase(iter);
}

void HttpRequest::ClearHeaders()
{
    headers_.swap(HttpHeaders());
}

HttpRequest::HttpRequest()
{
}

HttpRequest::~HttpRequest()
{
}

size_t HttpRequest::ReadChunk(void * blob, size_t size)
{
    return 0;
}

bool HttpRequest::GetContentLength(uint32_t & length)
{
    return false;
}

//HttpResponse
HttpResponse::HttpResponse()
    : status_code_(0)
{
}

HttpResponse::~HttpResponse()
{
}

size_t HttpResponse::WriteChunk(const void * blob, size_t size)
{
    return size;
}

int HttpResponse::GetStatusCode() const
{
    return status_code_;
}

bool HttpResponse::HasContentLength() const
{
    auto iter = headers_.find(kContentLength);
    return iter != headers_.end();
}

uint64_t HttpResponse::GetContentLength() const
{
    uint64_t length = 0;
    auto iter = headers_.find(kContentLength);
    if(iter != headers_.end())
        length = strtoui64(iter->second.data());
    return length;
}

bool HttpResponse::HasLastModified() const
{
    auto iter = headers_.find(kLastModified);
    return iter != headers_.end();
}

time_t HttpResponse::GetLastModified() const
{
    time_t date = -1;
    auto iter = headers_.find(kLastModified);
    if(iter != headers_.end())
        date = curl_parse_date(iter->second.data());
    return date;
}

bool HttpResponse::HasContentRange() const
{
    auto iter = headers_.find(kContentRange);
    return iter != headers_.end();
}

HttpRange HttpResponse::GetContentRange() const
{
    auto iter = headers_.find(kContentRange);
    if(iter == headers_.end())
        return HttpRange();

    size_t space = iter->second.find(' ');
    size_t hypen = iter->second.find('-');
    if(space >= hypen)
        return HttpRange();

    uint64_t first = strtoui64(&iter->second[space] + 1);
    uint64_t last = strtoui64(&iter->second[hypen] + 1);
    return HttpRange(first, static_cast<size_t>(last - first + 1));
}

void HttpResponse::GotHeader(const char * line, size_t length)
{
    int status_code = parse_status_code(line, length);
    if(status_code)
    {
        headers_.clear();
        status_code_ = status_code;
    }
    else
    {
        auto header = parse_header(line, length);
        if(!header.first.empty())
            headers_.insert(header);
    }
    return;
}

//HttpConnection
size_t HttpConnection::HeaderCallback(char * buffer, 
                                      size_t size, 
                                      size_t nitems, 
                                      void * param)
{
    size_t len = size * nitems;
    auto handler = reinterpret_cast<HttpConnection *>(param);
    if(!handler)
        return 0;
    if(!handler->response_)
        return len;

    handler->response_->GotHeader(buffer, size * nitems);
    return len;
}

size_t HttpConnection::ReadCallback(void * buffer, 
                                    size_t size, 
                                    size_t nitems, 
                                    void * param)
{
    auto handler = reinterpret_cast<HttpConnection *>(param);
    if(!handler)
        return 0;
    if(!handler->request_)
        return 0;

    size_t tranfered = handler->request_->ReadChunk(buffer, size * nitems);
    handler->io_stats_.out += tranfered;
    return tranfered;
}

size_t HttpConnection::WriteCallback(char * buffer, 
                                     size_t size, 
                                     size_t nitems, 
                                     void * param)
{
    auto handler = reinterpret_cast<HttpConnection *>(param);
    if(!handler)
        return 0;
    if(!handler->response_)
        return 0;

    size_t tranfered = handler->response_->WriteChunk(buffer, size * nitems);
    handler->io_stats_.in += tranfered;
    return tranfered;
}

/*HttpConnection*/
HttpConnection::HttpConnection()
    : curl_easy_(0), curl_multi_(0), 
      request_(0), response_(0)
{
}

HttpConnection::~HttpConnection()
{
    fini();
}

bool HttpConnection::init()
{
    if(!LazyInitialize())
        return false;
    return true;
}

void HttpConnection::fini()
{
    if(curl_easy_)
    {
        curl_easy_clean_headers(curl_easy_);
        curl_easy_cleanup(curl_easy_);
        curl_easy_ = 0;
    }

    if(curl_multi_)
    {
        curl_multi_cleanup(curl_multi_);
        curl_multi_ = 0;
    }
}

bool HttpConnection::LazyInitialize()
{
    if(!curl_easy_)
    {
        curl_easy_ = curl_easy_init();
        if(!curl_easy_)
            return false;
        
        curl_easy_setopt(curl_easy_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_easy_, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl_easy_, CURLOPT_READFUNCTION, ReadCallback);
        curl_easy_setopt(curl_easy_, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(curl_easy_, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl_easy_, CURLOPT_READDATA, this);
        curl_easy_setopt(curl_easy_, CURLOPT_FILETIME, 1);
        curl_easy_setopt(curl_easy_, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl_easy_, CURLOPT_PRIVATE, -1);
        Reset();
    }
    return true;
}

void HttpConnection::Reset()
{
    if(curl_easy_)
    {
        if(curl_multi_)
            curl_multi_remove_handle(curl_multi_, curl_easy_);

        curl_easy_setopt(curl_easy_, CURLOPT_PRIVATE, -1);

        SetRequestMethod(HttpRequestMethod::kGet);
        SetVerb(0);
        SetConnectTimeout(16000);
        SetLowSpeedLimit(0, 0);
        SetMaxRedirection(-1);
        EnableRedirection(false);
        SetRequest(0);
        SetResponse(0);
    }
}

bool HttpConnection::SetUrl(const std::string & url)
{
    if(url.empty())
        return false;

    if(curl_easy_ == 0)
        return false;

    curl_easy_setopt(curl_easy_, CURLOPT_URL, url.data());

    return true;
}

const char * HttpConnection::GetUrl() const
{
    char * url = 0;
    if(curl_easy_)
        curl_easy_getinfo(curl_easy_, CURLINFO_EFFECTIVE_URL, &url);
    return url ? url : "";
}

void HttpConnection::SetAgent(const char * agent )
{
    if(!curl_easy_)
        return;

    curl_easy_setopt(curl_easy_, CURLOPT_USERAGENT, agent);
}


void HttpConnection::SetLowSpeedLimit(uint32_t speed, uint32_t time)
{
    if(!curl_easy_)
        return;

    curl_easy_setopt(curl_easy_, CURLOPT_LOW_SPEED_LIMIT, speed);
    curl_easy_setopt(curl_easy_, CURLOPT_LOW_SPEED_TIME, time);
}

void HttpConnection::SetRequestMethod(HttpRequestMethod::Value method)
{
    if(!curl_easy_)
        return;

    switch(method)
    {
    case HttpRequestMethod::kGet:
        curl_easy_setopt(curl_easy_, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl_easy_, CURLOPT_NOBODY, 0);
        break;
    case HttpRequestMethod::kHead:
        curl_easy_setopt(curl_easy_, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl_easy_, CURLOPT_NOBODY, 1);
        break;
    case HttpRequestMethod::kPost:
        curl_easy_setopt(curl_easy_, CURLOPT_POST, 1);
        curl_easy_setopt(curl_easy_, CURLOPT_NOBODY, 0);
        break;
    case HttpRequestMethod::kPut:
        curl_easy_setopt(curl_easy_, CURLOPT_POST, 1);
        curl_easy_setopt(curl_easy_, CURLOPT_NOBODY, 0);
        break;
    default:
        assert(0);
    }
}

void HttpConnection::SetVerb(const char * method)
{
    if(!curl_easy_)
        return;

    curl_easy_setopt(curl_easy_, CURLOPT_CUSTOMREQUEST, method);
}

void HttpConnection::SetMaxRedirection(int limit)
{
    if(!curl_easy_)
        return;

    curl_easy_setopt(curl_easy_, CURLOPT_MAXREDIRS, limit);
}

void HttpConnection::EnableRedirection(bool value)
{
    if(!curl_easy_)
        return;

    curl_easy_setopt(curl_easy_, CURLOPT_FOLLOWLOCATION, value ? 1 : 0);
}

void HttpConnection::SetConnectTimeout(int ms)
{
    if(!curl_easy_)
        return;
    curl_easy_setopt(curl_easy_, CURLOPT_CONNECTTIMEOUT_MS, ms);
}

void HttpConnection::SetRequest(HttpRequest * request)
{
    request_ = request;
}

void HttpConnection::SetResponse(HttpResponse * response)
{
    response_ = response;
}

HttpConnResult HttpConnection::Perform()
{
    io_stats_.in = io_stats_.out = 0;

    if(!curl_easy_)
        return kConnFail;
    if(curl_easy_in_multi(curl_easy_))
        return kConnFail;
    ConnSetup();
    CURLcode code = curl_easy_perform(curl_easy_);
    return TranslateCurlCode(code);
}

HttpConnResult HttpConnection::AsyncPerform()
{
    io_stats_.in = io_stats_.out = 0;

    if(!curl_easy_)
        return kConnFail;

    if(!curl_easy_in_multi(curl_easy_))
    {
        ConnSetup();

        curl_multi_ = curl_multi_ ? curl_multi_ : curl_multi_init();
        if(!curl_multi_)
            return kConnFail;
        if(curl_multi_add_handle(curl_multi_, curl_easy_))
            return kConnFail;
    }

    int running_count_ = 0;
    CURLMcode multi_code = CURLM_CALL_MULTI_PERFORM;

    while(multi_code == CURLM_CALL_MULTI_PERFORM)
        multi_code = curl_multi_perform(curl_multi_, &running_count_);

    if(multi_code != CURLM_OK)
        return kConnFail;

    if(running_count_)
        return kConnAgain;

    int dont_care = 0;
    auto info = curl_multi_info_read(curl_multi_, &dont_care);
    if(info)
        curl_easy_setopt(curl_easy_, CURLOPT_PRIVATE, info->data.result);
        
    CURLcode code;
    curl_easy_getinfo(curl_easy_, CURLINFO_PRIVATE, &code);
    return TranslateCurlCode(code);
}

size_t HttpConnection::InSize() const
{
    return io_stats_.in;
}

size_t HttpConnection::OutSize() const
{
    return io_stats_.out;
}

void HttpConnection::Wait(uint32_t ms)
{
    if(curl_easy_in_multi(curl_easy_))
        curl_multi_wait(curl_multi_, 0, 0, ms, 0);
    return;
}

void HttpConnection::ConnSetup()
{
    if(!curl_easy_)
        return;

    curl_easy_clean_headers(curl_easy_);

    if(request_)
    {
        //Applying Headers
        for(auto iter = request_->headers_.begin(); 
            iter != request_->headers_.end();
            ++iter)
        {
            const size_t kMaxLineSize = 0x800;
            char line[kMaxLineSize];
            auto & key = iter->first;
            auto & value = iter->second;
            size_t line_length = key.length() + value.length() + 2;
            if(line_length >= kMaxLineSize)
                continue;
            sprintf_s(line, "%s: %s", key.data(), value.data());
            curl_easy_append_header(curl_easy_, line);
        }
        //Set post size
        uint32_t length = 0;
        if(request_->GetContentLength(length))
            curl_easy_setopt(curl_easy_, CURLOPT_POSTFIELDSIZE, length);
    }
    return;
}

}