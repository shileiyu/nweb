#ifndef NWEB_HTTP_HANDLER_H_
#define NWEB_HTTP_HANDLER_H_

#include "nweb.h"

namespace nweb
{

class URL;

enum HttpConnResult
{
    kConnPartialDone = -3,
    kConnTimeout = -2,
    kConnFail = -1,
    kConnOK = 0,
    kConnAgain,    
};

namespace HttpRequestMethod
{
enum Value
{
    kHead,
    kGet,
    kPost,
    kPut,
};
}

namespace HttpStatusCode
{
enum Value
{
    kNone = 0,
    kOK = 200,
    kPartialContent = 206,
    kIMUsed = 226,
    kMultipleChoices = 300,
    kMoved = 301,
    kFound = 302,
    kNotModified = 304,
    kPermanentRedirect = 308,
    kBadRequest = 400,
    kUnauthorized = 401,
    kForbidden = 403,
    kNotFound = 404,
    kNginxClientClosedRequest = 499,
    kInternalServerError = 500,
    kOptionNotSupported = 551,
};

const int kSuccessFirst = kOK;
const int kSuccessLast = kIMUsed;
const int kRedirectionFirst = kMultipleChoices;
const int kRedirectionLast = kPermanentRedirect;
const int kCLientErrorFirst = kBadRequest;
const int kCLientErrorLast = kNginxClientClosedRequest;
const int kServerErrorFirst = kBadRequest;
const int kServerErrorLast = kOptionNotSupported;
}

typedef std::map<std::string, std::string> HttpHeaders;

class HttpRange
{
public:
    HttpRange();

    HttpRange(uint64_t offset, size_t size);

    HttpRange(const HttpRange & obj);

    HttpRange & operator=(const HttpRange & obj);

    uint64_t offset() const;

    size_t size() const;

    uint64_t first() const;

    uint64_t last() const;

private:
    uint64_t offset_;
    size_t size_;
};

class HttpRequest
{
    friend class HttpConnection;
public:
    HttpRequest();
    virtual ~HttpRequest();

    void SetRange(uint64_t first, uint64_t last);
    void SetRange(const HttpRange & range);
    void AddHeader(const char * key, const char * value);
    void RemoveHeader(const char * key);
    void ClearHeaders();
protected:
    
    virtual size_t ReadChunk(void * blob, size_t size);
    virtual bool GetContentLength(uint32_t & length);
protected:
    HttpHeaders headers_;
};

class HttpResponse
{
    friend class HttpConnection;
public:
    HttpResponse();
    virtual ~HttpResponse();

    int GetStatusCode() const;
    bool HasContentLength() const;
    uint64_t GetContentLength() const;
    bool HasLastModified() const;
    time_t GetLastModified() const;
    bool HasContentRange() const;
    HttpRange GetContentRange() const;
protected:
    virtual size_t WriteChunk(const void * blob, size_t size);
private:
    void GotHeader(const char * line, size_t length);
protected:
    int status_code_;
    HttpHeaders headers_;
};

class HttpConnection
{
public:
    struct IOStats
    {
        size_t in;
        size_t out;
    };

    HttpConnection();

    ~HttpConnection();

    bool init();

    void fini();

    bool LazyInitialize();

    void Reset();

    //I won't escape the url you passed
    bool SetUrl(const std::string & url);

    const char * GetUrl() const;

    //If speed rate lower than [speed] BPS during [time] secondes.
    //Timeout error will occure.
    void SetLowSpeedLimit(uint32_t speed,uint32_t time);

    //Method indicate the beheiver of this handler.
    void SetRequestMethod(HttpRequestMethod::Value method);

    //Each request method is indecated by a verb('GET','POST', 'HEAD', 'PUT').
    //SetVerb allow you replace the default verb. 
    //Beware change the verb would not effected to the method has set.
    //internal workflow still subject to reqeuest method.
    void SetVerb(const char * method);

    void SetAgent(const char * agent);

    //Setting the limit to 0 will make library refuse any redirect. 
    //Set it to -1 for an infinite number of redirects (which is the default)
    void SetMaxRedirection(int limit);

    //A parameter set to true tells the library to follow any location.
    void EnableRedirection(bool value);

    void SetConnectTimeout(int ms);

    void SetRequest(HttpRequest * request);

    void SetResponse(HttpResponse * response);

    HttpConnResult Perform();

    HttpConnResult AsyncPerform();

    size_t InSize() const;

    size_t OutSize() const;

    void Wait(uint32_t ms);

private:
    static size_t HeaderCallback(char * buffer,
                                 size_t size,
                                 size_t nitems, 
                                 void * param);

    static size_t ReadCallback(void * buffer,
                               size_t size,
                               size_t nitems,
                               void * param);

    static size_t WriteCallback(char * buffer,
                                size_t size,
                                size_t nitems,
                                void * param);

    void ConnSetup();

private:
    void * curl_easy_;
    void * curl_multi_;
    HttpRequest * request_;
    HttpResponse * response_;
    IOStats io_stats_;
};

}

#endif