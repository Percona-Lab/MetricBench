// Thanks to Tom on stackoverflow
// http://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform

#pragma once

#include <string>
#include <algorithm>

class Uri
{
public:
  std::wstring QueryString, Path, Protocol, Host, Port;
  static Uri Parse(const std::wstring &);
};

