#include "error-handler.h"
#include <cstdlib>
#include <stdio.h>

bool *appShouldCloseRef;
const char *wm_result_to_string(WM_RESULT result) {
  switch (result) {
  case WM_SUCCESS:
    return "SUCCESS";
  case WM_ERR_INVALID_VALUE:
    return "INVALID_VALUE";
  case WM_ERR_INVALID_HANDLE:
    return "INVALID_WINDOW_HANDLE";
  case WM_ERR_NOT_PERMITED:
    return "OPERATION_NOT_PERMITTED";
  case WM_ERR_OUT_OF_MEMORY:
    return "OUT_OF_MEMORY";
  case WM_ERR_UNKNOWN:
    return "UNKNOWN_ERROR";
  default:
    return "UNRECOGNIZED_ERROR_CODE";
  }
}

WM_RESULT processErrorCode(uint32_t errorCode);

void setAppShouldClose(bool *_appShouldClose) {
  appShouldCloseRef = _appShouldClose;
}

void logWindowError(ErrorType errorType, uint32_t errorCode, uint32_t window,
                    const char *errorText, void *freePtr) {
  switch (errorType) {
  case WM_ERR_TYPE_FATAL: {
    printf("\033[31mFATAL : WINDOW (%u) %s, (ERROR CODE %s)\nSHUTTING "
           "DOWN...\033[0m\n",
           window, errorText, wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    exit(-1);
  }
  case WM_ERR_TYPE_WARNING: {
    printf("\033[33mWARNING: WINDOW (%u) %s, (ERROR %s)\nSHUTTING "
           "DOWN...\033[0m\n",
           window, errorText, wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    *appShouldCloseRef = true;
    return;
  }
  case WM_ERR_TYPE_RECOVERABLE: {
    printf("NOTE: WINDOW (%u) %s, (%s)\n", window, errorText,
           wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    return;
  }
  }
}

void logErrorWithCode(ErrorType errorType, uint32_t errorCode,
                      const char *errorText, void *freePtr) {
  switch (errorType) {
  case WM_ERR_TYPE_FATAL: {
    printf("\033[31mFATAL : %s, (ERROR %s)\nSHUTTING "
           "DOWN...\033[0m\n",
           errorText, wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    exit(-1);
  }
  case WM_ERR_TYPE_WARNING: {
    printf("\033[33mWARNING: %s, (ERROR %s)\nSHUTTING "
           "DOWN...\033[0m\n",
           errorText, wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    *appShouldCloseRef = true;
    return;
  }
  case WM_ERR_TYPE_RECOVERABLE: {
    printf("NOTE: %s, (%s)\n", errorText,
           wm_result_to_string(processErrorCode(errorCode)));
    if (freePtr)
      free(freePtr);
    return;
  }
  }
}

void logError(ErrorType errorType, const char *errorText, void *freePtr) {
  switch (errorType) {
  case WM_ERR_TYPE_FATAL: {
    printf("\033[31mFATAL : %s\nSHUTTING "
           "DOWN...\033[0m\n",
           errorText);
    if (freePtr)
      free(freePtr);
    exit(-1);
  }
  case WM_ERR_TYPE_WARNING: {
    printf("\033[33mWARNING: %s\nSHUTTING "
           "DOWN...\033[0m\n",
           errorText);
    if (freePtr)
      free(freePtr);
    *appShouldCloseRef = true;
    return;
  }
  case WM_ERR_TYPE_RECOVERABLE: {
    printf("NOTE: %s\n", errorText);
    if (freePtr)
      free(freePtr);
    return;
  }
  }
}

WM_RESULT processErrorCode(uint32_t errorCode) {
  switch (errorCode) {
  case 0:
    return WM_SUCCESS;
  case 2:
    return WM_ERR_INVALID_VALUE;
  case 3:
    return WM_ERR_INVALID_HANDLE;
  case 10:
    return WM_ERR_NOT_PERMITED;
  case 11:
    return WM_ERR_OUT_OF_MEMORY;
  default:
    return WM_ERR_UNKNOWN;
  }
}
