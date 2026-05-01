#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <Metal/Metal.h>
#include <cstdint>
#include <cstring>

namespace gal {

struct RenderPipelineStateCreateInfo {
  id<MTLDevice> device;
  id<MTLLibrary> metalLibrary;
  MTLPixelFormat pixelFormat;
  uint32_t rasterSampleCount = 1;
};

inline MTL4LibraryFunctionDescriptor *
makeLibraryFunction(id<MTLLibrary> metalLibrary, NSString *name) {
  @autoreleasepool {
    MTL4LibraryFunctionDescriptor *libraryFunction;
    libraryFunction = [MTL4LibraryFunctionDescriptor new];
    libraryFunction.library = metalLibrary;
    libraryFunction.name = name;
    return libraryFunction;
  }
}

core::Result<id<MTLRenderPipelineState>> inline createRenderPipelineState(
    const RenderPipelineStateCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLRenderPipelineState>>{
          .error =
              "The Metal Device can't create a render pipeline state : Invalid "
              "Metal Device",
          .ok = false};
    }

    if (createInfo.metalLibrary == nil) {
      return core::Result<id<MTLRenderPipelineState>>{
          .error =
              "The Metal Device can't create a render pipeline state : Invalid "
              "Metal Library",
          .ok = false};
    }
    NSError *error;

    id<MTL4Compiler> compiler = [createInfo.device
        newCompilerWithDescriptor:[MTL4CompilerDescriptor new]
                            error:&error];

    // Verify compiler creation
    const char *err = checkResource(compiler, @"compiler", -1, error);
    if (err) {
      return core::Result<id<MTLRenderPipelineState>>{.error = err,
                                                      .ok = false};
    }

    MTL4RenderPipelineDescriptor *descriptor =
        [MTL4RenderPipelineDescriptor new];
    descriptor.label = @"Render Pipeline";

    descriptor.colorAttachments[0].pixelFormat = createInfo.pixelFormat;
    descriptor.rasterSampleCount = createInfo.rasterSampleCount;

    if (err) {
      return core::Result<id<MTLRenderPipelineState>>{.error = err,
                                                      .ok = false};
    }

    descriptor.vertexFunctionDescriptor =
        makeLibraryFunction(createInfo.metalLibrary, @"vertexMain");
    descriptor.fragmentFunctionDescriptor =
        makeLibraryFunction(createInfo.metalLibrary, @"fragmentMain");

    id<MTLRenderPipelineState> renderPipelineState =
        [compiler newRenderPipelineStateWithDescriptor:descriptor
                                   compilerTaskOptions:nil
                                                 error:&error];

    err = checkResource(renderPipelineState, @"pipeline state", -1, error);
    if (err) {
      return core::Result<id<MTLRenderPipelineState>>{.error = err,
                                                      .ok = false};
    }

    [compiler release];

    return core::Result<id<MTLRenderPipelineState>>{
        .value = renderPipelineState, .ok = true};
  }
}
} // namespace gal
