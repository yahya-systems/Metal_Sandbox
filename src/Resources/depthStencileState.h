#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <Metal/Metal.h>
#include <cstdint>

namespace gal {

struct DepthStencileStateCreateInfo {
  id<MTLDevice> device;
  MTLCompareFunction depthCompareFunction = MTLCompareFunctionLessEqual;
  bool depthWriteEnabled = true;
};

inline core::Result<id<MTLDepthStencilState>>
createDepthStencileState(const DepthStencileStateCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLDepthStencilState>>{
          .error = strdup("The Metal device can't create a depth stencile "
                          "state : Invalid Metal Device"),
          .ok = false};
    }

    MTLDepthStencilDescriptor *descriptor = [MTLDepthStencilDescriptor new];
    descriptor.depthCompareFunction = createInfo.depthCompareFunction;
    descriptor.depthWriteEnabled = createInfo.depthWriteEnabled;
    id<MTLDepthStencilState> depthStencileState =
        [createInfo.device newDepthStencilStateWithDescriptor:descriptor];
    [descriptor release];
    if (depthStencileState == nil) {
      return core::Result<id<MTLDepthStencilState>>{
          .error = strdup("failed to create depth stencil state"), .ok = false};
    }

    return core::Result{.value = depthStencileState, .ok = true};
  }
}

} // namespace gal
