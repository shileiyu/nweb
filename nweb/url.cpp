#include "url.h"

namespace nweb
{

static bool IsUpperHex(char c)
{
    return 'A' <= c && c <= 'F';
}

static bool IsLowerHex(char c)
{
    return 'a' <= c && c <= 'f';
}

static bool IsNumber(char c)
{
    return '0' <= c && c <= '9';
}

static bool IsUppercase(char c)
{
    return 'A' <= c && c <= 'Z';
}

static bool IsLowercase(char c)
{
    return 'a' <= c && c <= 'z';
}

static bool IsURLSafeChar(char c)
{
    return c == '_' || c == '+' || c == '-' || c == '.';
}

static uint16_t HexToChar(uint8_t hex)
{
    uint8_t lo = 0;
    uint8_t hi = 0;
    lo = hex & 0xF;
    hi = (hex >> 4) & 0xF;

    lo += (0 <= lo && lo <= 9)?0x30:0x37;
    hi +=(0 <= hi && hi <= 9)?0x30:0x37;

    return (lo << 8) | hi;
}

static uint8_t CharToHex(uint16_t hex)
{
    uint8_t out = 0;
    uint8_t digit = 0;

    digit = static_cast<uint8_t>(hex);
    if(digit >= 'A' && digit <= 'F')
        out |= digit - 'A';
    else if (digit >= 'a' && digit <= 'f')
        out |= digit - 'a';
    else if(digit >= '0' && digit <= '9')
        out |= digit - '0';
    out <<= 4;

    digit = static_cast<uint8_t>(hex >> 8);
    if(digit >= 'A' && digit <= 'F')
        out |= digit - 'A';
    else if (digit >= 'a' && digit <= 'f')
        out |= digit - 'a';
    else if(digit >= '0' && digit <= '9')
        out |= digit - '0';

    return out;
}

static std::string UrlEncode(const char * in, size_t in_len)
{
    std::string escaped;
    if(in_len == 0)
        return escaped;

    size_t tci = 0;
    escaped.reserve(in_len);

    for(size_t i = 0; i < in_len; ++i)
    {
        char c = in[i];
        if( IsNumber(c)    ||
            IsUppercase(c) ||
            IsLowercase(c) ||
            IsURLSafeChar(c))
        {
            escaped += c;
        }
        else
        {
            uint16_t hex = HexToChar(c);
            escaped += '%';
            escaped += reinterpret_cast<char*>(&hex)[0];
            escaped += reinterpret_cast<char*>(&hex)[1];
        }
    }

    return std::move(escaped);
}

static std::string UrlEncode(const char * in)
{
    return UrlEncode(in, strlen(in));
}

static std::string UrlDecode(const char * in, size_t in_len)
{
    std::string unescaped;
    if(in_len == 0)
        return unescaped;
    size_t tci = 0;

    unescaped.reserve(in_len);

    for(size_t i = 0; i < in_len;)
    {
        if(in[i] == '%' && in_len - i >= 3)
        {
            uint16_t ch = *reinterpret_cast<const uint16_t*>(&in[i + 1]);
            unescaped += CharToHex(ch);
            i += 3;
        }
        else
        {
            unescaped += in[i];
            i += 1;
        }
    }
    return std::move(unescaped);
}

/*
 * Check whether the character is permitted in scheme string
 */
static int is_scheme_char(int c)
{
    return (!isalpha(c) && '+' != c && '-' != c && '.' != c) ? 0 : 1;
}

bool URL::Parse(const std::string & url)
{
    return Parse(url.data());
}

/*
Base on 'The URL and The Parser Implementation' by Hirochika Asai.
From http://draft.scyphus.co.jp/lang/c/url_parser.html.
*/
bool URL::Parse(const char * url)
{
    const char *tmpstr;
    const char *curstr;
    int len;
    int i;
    int userpass_flag;
    int bracket_flag;

    scheme_.clear();
    host_.clear();
    port_.clear();
    path_.clear();
    query_.clear();
    fragment_.clear();
    username_.clear();
    password_.clear();

    if(url == nullptr)
        return false;

    curstr = url;
    /*
    * <scheme>:<scheme-specific-part>
    * <scheme> := [a-z\+\-\.]+
    *             upper case = lower case for resiliency
    */
    /* Read scheme */
    tmpstr = strchr(curstr, ':');
    if ( NULL == tmpstr )
    {
        return false;
    }

    /* Get the scheme length */
    len = tmpstr - curstr;
    /* Check restrictions */
    for ( i = 0; i < len; i++ ) 
    {
        if ( !is_scheme_char(curstr[i]) ) 
        {
            return false;
        }
    }
    /* Copy the scheme to the storage */
    scheme_.assign(curstr, len);
    /* Skip ':' */
    tmpstr++;
    curstr = tmpstr;

    /*
    * //<user>:<password>@<host>:<port>/<url-path>
    * Any ":", "@" and "/" must be encoded.
    */
    /* Eat "//" */
    for ( i = 0; i < 2; i++ )
    {
        if ( '/' != *curstr )
        {
            return false;
        }
        curstr++;
    }

    /* Check if the user (and password) are specified. */
    userpass_flag = 0;
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) 
    {
        if ( '@' == *tmpstr ) 
        {
            /* Username and password are specified */
            userpass_flag = 1;
            break;
        } 
        else if ( '/' == *tmpstr ) 
        {
            /* End of <host>:<port> specification */
            userpass_flag = 0;
            break;
        }
        tmpstr++;
    }

    /* User and password specification */
    tmpstr = curstr;
    if ( userpass_flag ) 
    {
        /* Read username */
        while ( '\0' != *tmpstr && ':' != *tmpstr && '@' != *tmpstr ) 
        {
            tmpstr++;
        }
        len = tmpstr - curstr;
        username_.assign(curstr, len);
        /* Proceed current pointer */
        curstr = tmpstr;
        if ( ':' == *curstr ) 
        {
            /* Skip ':' */
            curstr++;
            /* Read password */
            tmpstr = curstr;
            while ( '\0' != *tmpstr && '@' != *tmpstr ) 
            {
                tmpstr++;
            }
            len = tmpstr - curstr;
            password_.assign(curstr, len);
            curstr = tmpstr;
        }
        /* Skip '@' */
        if ( '@' != *curstr ) 
            return false;

        curstr++;
    }

    if ( '[' == *curstr ) 
    {
        bracket_flag = 1;
    } 
    else 
    {
        bracket_flag = 0;
    }

    /* Proceed on by delimiters with reading host */
    tmpstr = curstr;
    while ( '\0' != *tmpstr ) 
    {
        if ( bracket_flag && ']' == *tmpstr ) 
        {
            /* End of IPv6 address. */
            tmpstr++;
            break;
        } 
        else if ( !bracket_flag && (':' == *tmpstr || '/' == *tmpstr) ) 
        {
            /* Port number is specified. */
            break;
        }
        tmpstr++;
    }
    len = tmpstr - curstr;
    host_.assign(curstr, len);
    curstr = tmpstr;

    /* Is port number specified? */
    if ( ':' == *curstr ) 
    {
        curstr++;
        /* Read port number */
        tmpstr = curstr;
        while ( '\0' != *tmpstr && '/' != *tmpstr ) 
        {
            tmpstr++;
        }
        len = tmpstr - curstr;
        port_.assign(curstr, len);
        curstr = tmpstr;
    }

    /* End of the string */
    if ( '\0' == *curstr ) 
    {
        return true;
    }

    /* Skip '/' */
    if ( '/' != *curstr )
    {
        return false;
    }
    curstr++;

    /* Parse path */
    tmpstr = curstr;
    while ( '\0' != *tmpstr && '#' != *tmpstr  && '?' != *tmpstr ) 
    {
        tmpstr++;
    }
    len = tmpstr - curstr;
    path_.assign(curstr, len);
    curstr = tmpstr;

    /* Is query specified? */
    if ( '?' == *curstr ) 
    {
        DictEntry param;
        while('\0' != *curstr && '#' != *curstr)
        {
            /* Skip '?' or '&' */
            curstr++;
            /* Read query */
            tmpstr = curstr;
            /* Read key*/
            while('\0' != *tmpstr)
            {
                if('&' == *tmpstr)
                    break;
                if('=' == *tmpstr)
                    break;
                tmpstr++;
            }
            if('\0' == *tmpstr || '&' == *tmpstr)
                return false;
            param.first = UrlDecode(curstr, tmpstr - curstr);
            tmpstr++;
            curstr = tmpstr;
            while('\0' != *tmpstr)
            {
                if('=' == *tmpstr)
                    break;
                if('&' == *tmpstr)
                    break;
                if('#' == *tmpstr)
                    break;
                tmpstr++;
            }
            param.second = UrlDecode(curstr, tmpstr - curstr);
            query_.insert(param);
            curstr = tmpstr;
            /* Read value*/
        }
    }

    /* Is fragment specified? */
    if ( '#' == *curstr ) 
    {
        /* Skip '#' */
        curstr++;
        /* Read fragment */
        tmpstr = curstr;
        while ( '\0' != *tmpstr ) 
        {
            tmpstr++;
        }
        len = tmpstr - curstr;
        fragment_.assign(curstr, len);
        curstr = tmpstr;
    }

    return true;
}

URL::URL()
{
}

URL::URL(const char * url)
{
    Parse(url);
}

URL::URL(const std::string & url)
{
    Parse(url.data());
}

URL::~URL()
{
}

bool URL::IsValid() const
{
    if(scheme_.empty())
        return false;
    if(host_.empty())
        return false;
    return true;
}

std::string URL::Escaped() const
{
    std::string escaped;
    escaped += scheme_;
    escaped += "://";

    if(!username_.empty() && !password_.empty())
    {
        escaped += username_;
        escaped += ':';
        escaped += password_;
        escaped += '@';
    }

    escaped += host_;

    if(!path_.empty())
    {
        escaped += '/';
        escaped += path_;
    }
    if(!query_.empty())
    {
        escaped += '?';
        for(auto param = query_.begin(); param != query_.end();)
        {
            escaped += param->first;
            escaped += '=';
            escaped += param->second;
            param++;
            if(param != query_.end())
                escaped += '&';
        }
    }
    if(!fragment_.empty())
    {
        escaped += '#';
        escaped += fragment_;
    }

    return escaped;
}

void URL::AppendPath(const char * part)
{
    if(!path_.empty())
        path_ += '/';
    path_ += UrlEncode(part);
}

void URL::ClearPath()
{
    path_.clear();
}

void URL::AddParam(const char * key, const char * value)
{
    DictEntry param;
    param.first = UrlEncode(key);
    param.second = UrlEncode(value);
    query_.insert(param);
}

void URL::RemoveParam(const char * key)
{
    query_.erase(key);
}

}