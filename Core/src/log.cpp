#include "core/log.hpp"
#include <source_location>

namespace core {

void log(LogType type, const char *msg, ...) {
  va_list args;
  va_start(args, msg);

  char buffer[128];
  vsnprintf(buffer, 128, msg, args);

  switch (type) {
  case LOG_FATAL:
    fprintf(stderr, "\033[31mFATAL : %s\n\033[0m", buffer);
    break;
  case LOG_WARNING:
    fprintf(stderr, "\033[33mWARNING : %s\n\033[0m", buffer);
    break;
  case LOG_NOTE:
    fprintf(stdout, "NOTE : %s", buffer);
    break;
  }

  va_end(args);
}

} // namespace core
