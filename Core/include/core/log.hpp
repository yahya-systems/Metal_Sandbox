#pragma once
#include <cstdarg>
#include <cstdio>
#include <stdarg.h>
#include <stdio.h>

namespace core {

enum LogType {
  LOG_FATAL,
  LOG_WARNING,
  LOG_NOTE,
};

void log(LogType type, const char *msg, ...);

} // namespace core
