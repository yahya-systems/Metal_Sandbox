#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <cstdint>

namespace gal {

struct CommandAllocatorCreateInfo {
  id<MTLDevice> device;
};

core::Result<id<MTL4CommandAllocator>> inline createCommandAllocator(
    const CommandAllocatorCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTL4CommandAllocator>>{
          .error = strdup("The Metal Device can't create a command allocator : "
                          "Invalid Metal Device"),
          .ok = false};
    }

    id<MTL4CommandAllocator> allocator;
    MTL4CommandAllocatorDescriptor *descriptor =
        [MTL4CommandAllocatorDescriptor new];
    NSError *error;
    allocator = [createInfo.device newCommandAllocatorWithDescriptor:descriptor
                                                               error:&error];
    const char *result =
        checkResource(allocator, @"command allocator", -1, error);
    if (result) {
      return core::Result<id<MTL4CommandAllocator>>{.error = result,
                                                    .ok = false};
    }

    return core::Result<id<MTL4CommandAllocator>>{.value = allocator,
                                                  .ok = true};
  }
}
} // namespace gal
