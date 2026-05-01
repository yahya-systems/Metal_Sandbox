#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <cstdint>
#include <cstring>

namespace gal {

struct LibraryCreateInfo {
  id<MTLDevice> device;
  const char *URL;
};

inline core::Result<id<MTLLibrary>>
loadMetalLibrary(const LibraryCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLLibrary>>{
          .error = strdup("The Metal device can't create a metal library : "
                          "Invalid Metal Device"),
          .ok = false};
    }

    if (createInfo.URL == nullptr) {
      return core::Result<id<MTLLibrary>>{
          .error = strdup(
              "The Metal device can't create a metal library : Invalid URL"),
          .ok = false};
    }

    NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];

    NSError *error;

    NSURL *baseURL = [NSURL fileURLWithPath:cwd isDirectory:YES];

    NSString *url = [NSString stringWithUTF8String:createInfo.URL];

    NSURL *libURL = [NSURL URLWithString:url relativeToURL:baseURL];

    id<MTLLibrary> metalLib = [createInfo.device newLibraryWithURL:libURL
                                                             error:&error];

    const char *err = checkResource(metalLib, @"metal library", -1, error);

    if (err) {
      return core::Result<id<MTLLibrary>>{.error = err, .ok = false};
    }

    return core::Result<id<MTLLibrary>>{.value = metalLib, .ok = true};
  }
}
} // namespace gal
