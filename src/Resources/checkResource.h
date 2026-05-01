#pragma once
#include <Metal/Metal.h>

inline const char *checkResource(id resource, NSString *name, long number,
                                 NSError *error) {
  if (resource) {
    return nullptr;
  }

  NSMutableString *errorString;
  errorString =
      [NSMutableString stringWithString:@"The Metal device can't create"];

  [errorString appendFormat:@" %@", name];

  if (number >= 0) {
    [errorString appendFormat:@ "%ld", number];
  }

  if (error != nil) {
    [errorString appendFormat:@": %@\n", error];
  } else {
    [errorString appendString:@"."];
  }
  NSUInteger len =
      [errorString lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
  char *myBuffer = (char *)malloc(len); // Or use your custom allocator!

  [errorString getCString:myBuffer maxLength:len encoding:NSUTF8StringEncoding];
  return myBuffer;
}
