#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include <cstdint>

namespace gal {

struct ResidencySetCreateInfo {
  id<MTLDevice> device;
};

core::Result<id<MTLResidencySet>> inline createResidencySet(
    const ResidencySetCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLResidencySet>>{
          .error = "The Metal Device can't create a residency set : Invalid "
                   "Metal Device",
          .ok = false};
    }
    NSError *error = nil;

    MTLResidencySetDescriptor *residencySetDescriptor;

    residencySetDescriptor = [MTLResidencySetDescriptor new];

    id<MTLResidencySet> residencySet;
    residencySet =
        [createInfo.device newResidencySetWithDescriptor:residencySetDescriptor
                                                   error:&error];

    const char *err = checkResource(residencySet, @"residency set", -1, error);

    if (err) {
      return core::Result<id<MTLResidencySet>>{.error = err, .ok = false};
    }

    return core::Result<id<MTLResidencySet>>{.value = residencySet, .ok = true};
  }
}

} // namespace gal
