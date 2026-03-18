#include "handler/settings.h"
#include <string>

Settings global;

#ifndef SUBCONVERTER_USE_REAL_FILE_IMPL
bool fileExist(const std::string&, bool) { return false; }
std::string fileGet(const std::string&, bool) { return ""; }
#endif
