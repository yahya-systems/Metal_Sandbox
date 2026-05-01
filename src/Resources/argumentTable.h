#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <cstdint>

namespace gal {

struct ArgumentTableCreateInfo {
  id<MTLDevice> device;
  uint32_t maxBufferBindCount = 16;
  uint32_t maxTextureBindCount = 16;
};

core::Result<id<MTL4ArgumentTable>> inline createArgumentTable(
    const ArgumentTableCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTL4ArgumentTable>>{
          .error = "The Metal Device can't create an argument table : Invalid "
                   "Metal Device",
          .ok = false};
    }
    NSError *error;
    MTL4ArgumentTableDescriptor *argumentTableDescriptor;
    argumentTableDescriptor = [MTL4ArgumentTableDescriptor new];
    argumentTableDescriptor.maxBufferBindCount = createInfo.maxBufferBindCount;
    argumentTableDescriptor.maxTextureBindCount =
        createInfo.maxTextureBindCount;

    id<MTL4ArgumentTable> argumentTable;
    argumentTable = [createInfo.device
        newArgumentTableWithDescriptor:argumentTableDescriptor
                                 error:&error];

    [argumentTableDescriptor release];

    const char *errorBuffer =
        checkResource(argumentTable, @"argument table", -1, error);
    if (errorBuffer) { // Program will crash

      return core::Result<id<MTL4ArgumentTable>>{.error = errorBuffer,
                                                 .ok = false};
    }

    return core::Result<id<MTL4ArgumentTable>>{.value = argumentTable,
                                               .ok = true};
  }
}

} // namespace gal
