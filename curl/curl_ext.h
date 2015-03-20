#ifndef CURL_EXT_H_
#define CURL_EXT_H_

#include "curl.h"

#ifdef  __cplusplus
extern "C" 
{
#endif

CURL_EXTERN void curl_easy_clean_headers(void * curl_easy);

CURL_EXTERN void curl_easy_append_header(void * curl_easy, 
                                         const char * header);

CURL_EXTERN bool curl_easy_in_multi(void * curl_easy);

CURL_EXTERN time_t curl_parse_date(const char * date);

#ifdef  __cplusplus
}
#endif

#endif

