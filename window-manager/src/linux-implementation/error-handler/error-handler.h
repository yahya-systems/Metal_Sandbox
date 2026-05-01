#ifndef WM_ERROR_HANDLER
#define WM_ERROR_HANDLER
#include <cstdint>

enum ErrorType {
  WM_ERR_TYPE_FATAL,
  WM_ERR_TYPE_WARNING,
  WM_ERR_TYPE_RECOVERABLE,
};

enum WM_RESULT {
  WM_SUCCESS = 0,
  WM_ERR_INVALID_VALUE,
  WM_ERR_INVALID_HANDLE,
  WM_ERR_NOT_PERMITED,
  WM_ERR_OUT_OF_MEMORY,
  WM_ERR_UNKNOWN,
};

void setAppShouldClose(bool *appShouldClose);

void logWindowError(ErrorType errorType, uint32_t errorCode, uint32_t window,
                    const char *errorText, void *freePtr);
void logErrorWithCode(ErrorType errorType, uint32_t errorCode,
                      const char *errorText, void *freePtr);
void logError(ErrorType errorType, const char *errorText, void *freePtr);

#endif // WM_ERROR_HANDLER
