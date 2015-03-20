#include "curl_setup.h"
#include "urldata.h"
#include "parsedate.h"


void curl_easy_clean_headers(void * curl_easy)
{
    struct SessionHandle * data = (struct SessionHandle *)curl_easy;
    if(!data)
        return;

    if(data->set.headers)
    {
        curl_slist_free_all(data->set.headers);
        data->set.headers = 0;
    }
    return;
}

void curl_easy_append_header(void * curl_easy, const char * header)
{
    struct SessionHandle * data = (struct SessionHandle *)curl_easy;
    if(!data)
        return;

    data->set.headers = curl_slist_append(data->set.headers, header);
    return;
}

bool curl_easy_in_multi(void * curl_easy)
{
    struct SessionHandle *data = (struct SessionHandle *)curl_easy;
    if(!data)
        return false;
    return data->multi ? true : false;
}

time_t curl_parse_date(const char * date)
{
    time_t curl_getdate(const char *p, const time_t *now);

    time_t mock;
    return curl_getdate(date, &mock);
}

