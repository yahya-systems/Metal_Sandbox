#pragma once
#include "checkResource.h"
#include "command.h"
#include "core/core.hpp"
#include <Metal/Metal.h>
#include <cstdint>

namespace gal {

enum StorageMode {
  MODE_SHARED,
  MODE_PRIVATE,
};

struct BufferCreateInfo {
  id<MTLDevice> device;
  StorageMode storageMode;
  size_t length;
  const void *data;
};

inline void copyDataToPrivateBuffer(id<MTLBuffer> dstBuffer, int32_t dstOffset,
                                    id<MTLBuffer> srcBuffer, int32_t srcOffset,
                                    size_t length, id<MTLDevice> device) {
  @autoreleasepool {
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLCommandBuffer> cmd = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromBuffer:srcBuffer
             sourceOffset:srcOffset
                 toBuffer:dstBuffer
        destinationOffset:dstOffset
                     size:length];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    [queue release];
  }
}

inline core::Result<id<MTLBuffer>>
createBuffer(const BufferCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLBuffer>>{
          .error = strdup("The Metal device can't create a resource buffer : "
                          "Invalid Metal Device"),
          .ok = false};
    }
    int mode;

    switch (createInfo.storageMode) {
    case MODE_SHARED:
      mode = MTLResourceStorageModeShared;
      break;
    case MODE_PRIVATE:
      mode = MTLResourceStorageModePrivate;
      break;
    }

    id<MTLBuffer> buffer;
    if (createInfo.data) {
      buffer =
          [createInfo.device newBufferWithBytes:createInfo.data
                                         length:createInfo.length
                                        options:MTLResourceStorageModeShared];

      const char *err = checkResource(buffer, @"resource buffer", -1, nil);

      if (err) {
        return core::Result<id<MTLBuffer>>{.error = err, .ok = false};
      }

      if (createInfo.storageMode == MODE_PRIVATE) {
        id<MTLBuffer> temp = [createInfo.device
            newBufferWithLength:createInfo.length
                        options:MTLResourceStorageModePrivate];

        const char *err = checkResource(temp, @"resource buffer", -1, nil);

        if (err) {
          [buffer release];
          return core::Result<id<MTLBuffer>>{.error = err, .ok = false};
        }

        copyDataToPrivateBuffer(temp, 0, buffer, 0, createInfo.length,
                                createInfo.device);

        [buffer release];
        buffer = temp;
      }
    } else {
      buffer = [createInfo.device newBufferWithLength:createInfo.length
                                              options:mode];
    }

    const char *err = checkResource(buffer, @"resource buffer", -1, nil);

    if (err) {
      return core::Result<id<MTLBuffer>>{.error = err, .ok = false};
    }

    return core::Result<id<MTLBuffer>>{.value = buffer, .ok = true};
  }
}

inline core::Result<void *> mapBuffer(id<MTLBuffer> buffer) {
  void *mapPtr = [buffer contents];
  if (mapPtr == NULL) {
    return core::Result<void *>{.error = strdup("Failed to map buffer"),
                                .ok = false};
  }
  return core::Result<void *>{.value = mapPtr, .ok = true};
}

} // namespace gal
