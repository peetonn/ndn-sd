#ifndef __mime_hpp__
#define __mime_hpp__

#include <string>

namespace mime 
{

std::string content_type(const std::string &str);
std::string extension(const std::string &type);

}

#endif
