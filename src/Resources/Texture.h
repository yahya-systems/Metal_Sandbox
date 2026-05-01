#pragma once
#include "checkResource.h"
#include "core/core.hpp"
#include "stb/stb_image.h"
#include "tinyexr.h"
#include <Metal/Metal.h>
#include <cstdint>
#include <stdlib.h>
#include <string.h>

namespace gal {
struct TextureCreateInfo {
  id<MTLDevice> device;
  uint32_t width;
  uint32_t height;
  MTLTextureType textureType = MTLTextureType2D;
  MTLStorageMode storageMode = MTLStorageModeShared;
  MTLPixelFormat pixelFormat = MTLPixelFormatRGBA8Unorm;
  uint32_t sampleCount = 1;
};

inline bool endsWith(const char *filename, const char *suffix) {
  size_t lenFile = strlen(filename);
  size_t lenSuffix = strlen(suffix);

  if (lenSuffix > lenFile)
    return false;

  return strcmp(filename + lenFile - lenSuffix, suffix) == 0;
}

typedef enum {
  EXR_OUTPUT_GRAYSCALE,
  EXR_OUTPUT_RGB,
  EXR_OUTPUT_RGBA,
  EXR_OUTPUT_UNKNOWN
} EXROutputType;

static bool str_eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

inline bool load_exr(const char *filename, float **out_pixels, int *out_width,
                     int *out_height, EXROutputType *out_type,
                     int requested_channels) {
  EXRVersion version;
  if (ParseEXRVersionFromFile(&version, filename) != TINYEXR_SUCCESS)
    return false;

  EXRHeader header;
  InitEXRHeader(&header);

  const char *err = nullptr;
  if (ParseEXRHeaderFromFile(&header, &version, filename, &err) !=
      TINYEXR_SUCCESS) {
    FreeEXRErrorMessage(err);
    return false;
  }

  for (int i = 0; i < header.num_channels; i++)
    header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;

  EXRImage image;
  InitEXRImage(&image);

  if (LoadEXRImageFromFile(&image, &header, filename, &err) !=
      TINYEXR_SUCCESS) {
    FreeEXRErrorMessage(err);
    FreeEXRHeader(&header);
    return false;
  }

  *out_width = image.width;
  *out_height = image.height;
  const int n = image.width * image.height;

  bool ok = false;

  if (requested_channels == 1 || header.num_channels == 1) {
    *out_pixels = (float *)malloc(n * sizeof(float));
    *out_type = EXR_OUTPUT_GRAYSCALE;
    memcpy(*out_pixels, image.images[0], n * sizeof(float));
    ok = true;
  } else {
    int idx_r = -1, idx_g = -1, idx_b = -1, idx_a = -1;

    for (int i = 0; i < header.num_channels; i++) {
      const char *name = header.channels[i].name;
      if (str_eq(name, "R"))
        idx_r = i;
      if (str_eq(name, "G"))
        idx_g = i;
      if (str_eq(name, "B"))
        idx_b = i;
      if (str_eq(name, "A"))
        idx_a = i;
    }

    if (idx_r >= 0 && idx_g >= 0 && idx_b >= 0) {
      bool has_alpha = requested_channels == 4 && idx_a >= 0;
      int nc = has_alpha ? 4 : 3;
      *out_type = has_alpha ? EXR_OUTPUT_RGBA : EXR_OUTPUT_RGB;
      *out_pixels = (float *)malloc(n * nc * sizeof(float));

      float *r = (float *)image.images[idx_r];
      float *g = (float *)image.images[idx_g];
      float *b = (float *)image.images[idx_b];
      float *a = has_alpha ? (float *)image.images[idx_a] : nullptr;

      for (int i = 0; i < n; i++) {
        (*out_pixels)[i * nc + 0] = r[i];
        (*out_pixels)[i * nc + 1] = g[i];
        (*out_pixels)[i * nc + 2] = b[i];
        if (has_alpha)
          (*out_pixels)[i * nc + 3] = a[i];
      }
      ok = true;
    } else {
      *out_pixels = (float *)malloc(n * sizeof(float));
      *out_type = EXR_OUTPUT_GRAYSCALE;
      memcpy(*out_pixels, image.images[0], n * sizeof(float));
      ok = true;
    }
  }

  FreeEXRImage(&image);
  FreeEXRHeader(&header);
  return ok;
}

inline MTLPixelFormat exrOutputTypeToMTLPixelFormat(EXROutputType type) {
  switch (type) {
  case EXR_OUTPUT_GRAYSCALE:
    return MTLPixelFormatR32Float;
  case EXR_OUTPUT_RGB:
    return MTLPixelFormatRGBA32Float; // Metal has no RGB32Float
  case EXR_OUTPUT_RGBA:
    return MTLPixelFormatRGBA32Float;
  default:
    return MTLPixelFormatInvalid;
  }
}

inline void copyBufferToImage(id<MTLBuffer> srcBuffer, int bytesPerRow,
                              int length, int width, int height,
                              id<MTLTexture> dstTexture, id<MTLDevice> device) {
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];
  id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

  id<MTLBlitCommandEncoder> blitCommandEncoder =
      [commandBuffer blitCommandEncoder];
  [blitCommandEncoder copyFromBuffer:srcBuffer
                        sourceOffset:0
                   sourceBytesPerRow:bytesPerRow
                 sourceBytesPerImage:length
                          sourceSize:MTLSizeMake(width, height, 1)
                           toTexture:dstTexture
                    destinationSlice:0
                    destinationLevel:0
                   destinationOrigin:MTLOriginMake(0, 0, 0)];
  [blitCommandEncoder endEncoding];
  [commandBuffer commit];
  [commandBuffer waitUntilCompleted];
  [commandBuffer release];
  [commandQueue release];
}

core::Result<id<MTLTexture>> inline createTexture(
    const TextureCreateInfo &createInfo) {
  @autoreleasepool {
    if (createInfo.device == nil) {
      return core::Result<id<MTLTexture>>{
          .error = strdup(
              "The Metal Device can't create a texture : Invalid Metal Device"),
          .ok = false};
    }

    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor new];
    textureDescriptor.textureType = createInfo.textureType;
    textureDescriptor.pixelFormat = createInfo.pixelFormat;
    textureDescriptor.storageMode = createInfo.storageMode;
    textureDescriptor.width = createInfo.width;
    textureDescriptor.height = createInfo.height;

    if (createInfo.textureType == MTLTextureType2DMultisample) {
      textureDescriptor.sampleCount = createInfo.sampleCount;
    }

    id<MTLTexture> texture =
        [createInfo.device newTextureWithDescriptor:textureDescriptor];

    [textureDescriptor release];

    if (texture == nil) {
      return core::Result<id<MTLTexture>>{
          .error = strdup(
              "The Metal Device can't create a texture : Reason Unknown"),
          .ok = false};
    }

    return core::Result<id<MTLTexture>>{.value = texture, .ok = true};
  }
}

inline int mtlPixelFormatChannelCount(MTLPixelFormat format) {
  switch (format) {
  case MTLPixelFormatR8Unorm:
  case MTLPixelFormatR8Unorm_sRGB:
  case MTLPixelFormatR32Float:
    return 1;
  case MTLPixelFormatRG8Unorm:
  case MTLPixelFormatRG32Float:
    return 2;
  case MTLPixelFormatRGBA8Unorm:
  case MTLPixelFormatRGBA8Unorm_sRGB:
  case MTLPixelFormatRGBA32Float:
    return 4;
  default:
    return 0;
  }
}

core::Result<id<MTLTexture>> inline loadTextureFromFile(
    const char *url, MTLPixelFormat pixelFormat, MTLStorageMode storageMode,
    id<MTLDevice> device) {
  if (endsWith(url, ".png")) {
    int width, height;
    int desiredChannels = mtlPixelFormatChannelCount(pixelFormat);
    if (desiredChannels == 0) {
      return core::Result<id<MTLTexture>>{
          .error = strdup("The Metal Device can't create texture : "
                          "Unsupported Pixel Format"),
          .ok = false};
    }
    unsigned char *imageData =
        stbi_load(url, &width, &height, nullptr, desiredChannels);
    if (!imageData) {
      return core::Result<id<MTLTexture>>{
          .error = strdup(
              "The Metal Device can't create texture : Invalid Image URL"),
          .ok = false};
    }

    TextureCreateInfo textureCreateInfo{};
    textureCreateInfo.device = device;
    textureCreateInfo.textureType = MTLTextureType2D;
    textureCreateInfo.pixelFormat = pixelFormat;
    textureCreateInfo.storageMode = storageMode;
    textureCreateInfo.width = width;
    textureCreateInfo.height = height;
    core::Result<id<MTLTexture>> result = createTexture(textureCreateInfo);
    if (!result.ok) {
      stbi_image_free(imageData);
      return result;
    }

    id<MTLTexture> texture = result.unwrap();

    if (storageMode == MTLStorageModeShared) {
      MTLRegion replaceRegion = {{0, 0, 0},
                                 {(uint32_t)width, (uint32_t)height, 1}};

      [texture replaceRegion:replaceRegion
                 mipmapLevel:0
                   withBytes:imageData
                 bytesPerRow:width * desiredChannels];
      stbi_image_free(imageData);

    } else if (storageMode == MTLStorageModePrivate) {
      id<MTLBuffer> stagingBuffer =
          [device newBufferWithBytes:imageData
                              length:width * height * desiredChannels
                             options:MTLResourceStorageModeShared];
      stbi_image_free(imageData);
      if (stagingBuffer == nil) {
        return core::Result<id<MTLTexture>>{
            .error =
                strdup("The Metal Device can't create texture : out of memory"),
            .ok = false};
      }
      copyBufferToImage(stagingBuffer, width * desiredChannels,
                        width * height * desiredChannels, width, height,
                        texture, device);
      [stagingBuffer release];
    }

    return core::Result<id<MTLTexture>>{.value = texture, .ok = true};

  } else if (endsWith(url, ".exr")) {
    int width, height;
    float *imageData;
    EXROutputType outputType;
    int requestedChannels = mtlPixelFormatChannelCount(pixelFormat);
    if (pixelFormat != MTLPixelFormatR32Float &&
        pixelFormat != MTLPixelFormatRGBA32Float) {
      free(imageData);
      return core::Result<id<MTLTexture>>{
          .error = strdup("EXR textures must use a float pixel format"),
          .ok = false};
    }
    if (requestedChannels == 0) {
      return core::Result<id<MTLTexture>>{
          .error = strdup("The Metal Device can't create texture : Invalid "
                          "Requested Format"),
          .ok = false};
    }
    if (!load_exr(url, &imageData, &width, &height, &outputType,
                  requestedChannels)) {
      return core::Result<id<MTLTexture>>{
          .error = strdup(
              "The Metal Device can't create texture : Invalid Image URL"),
          .ok = false};
    }

    TextureCreateInfo textureCreateInfo{};
    textureCreateInfo.device = device;
    textureCreateInfo.textureType = MTLTextureType2D;
    textureCreateInfo.pixelFormat = pixelFormat;
    textureCreateInfo.width = width;
    textureCreateInfo.height = height;
    textureCreateInfo.storageMode = storageMode;
    core::Result<id<MTLTexture>> result = createTexture(textureCreateInfo);
    if (!result.ok) {
      free(imageData);
      return core::Result<id<MTLTexture>>{
          .error =
              strdup("The Metal Device can't create texture : Error Unknown"),
          .ok = false};
    }
    id<MTLTexture> texture = result.unwrap();

    if (storageMode == MTLStorageModeShared) {
      MTLRegion replaceRegion = {{0, 0, 0},
                                 {(uint32_t)width, (uint32_t)height, 1}};

      [texture replaceRegion:replaceRegion
                 mipmapLevel:0
                   withBytes:imageData
                 bytesPerRow:width * requestedChannels * sizeof(float)];
      free(imageData);

    } else if (storageMode == MTLStorageModePrivate) {
      id<MTLBuffer> stagingBuffer = [device
          newBufferWithBytes:imageData
                      length:width * height * requestedChannels * sizeof(float)
                     options:MTLResourceStorageModeShared];
      free(imageData);
      if (stagingBuffer == nil) {
        return core::Result<id<MTLTexture>>{
            .error =
                strdup("The Metal Device can't create texture : out of memory"),
            .ok = false};
      }
      copyBufferToImage(stagingBuffer,
                        width * requestedChannels * sizeof(float),
                        width * height * requestedChannels * sizeof(float),
                        width, height, texture, device);
      [stagingBuffer release];
    }

    return core::Result<id<MTLTexture>>{.value = texture, .ok = true};
  } else {
    return core::Result<id<MTLTexture>>{
        .error = strdup(
            "The Metal Device can't create texture : Unsupported Image Type"),
        .ok = false};
  }
}

core::Result<id<MTLTexture>> inline loadCubemapTextureFromFiles(
    const char **urls, int width, int height, MTLPixelFormat pixelFormat,
    MTLStorageMode storageMode, id<MTLDevice> device) {
  TextureCreateInfo textureCreateInfo{};
  textureCreateInfo.device = device;
  textureCreateInfo.textureType = MTLTextureTypeCube;
  textureCreateInfo.pixelFormat = pixelFormat;
  textureCreateInfo.height = height;
  textureCreateInfo.width = width;
  core::Result<id<MTLTexture>> result = createTexture(textureCreateInfo);
  if (!result.ok) {
    return result;
  }
  id<MTLTexture> texture = result.unwrap();
  unsigned char *imageDatas[6];
  int desiredChannels = mtlPixelFormatChannelCount(pixelFormat);
  if (desiredChannels == 0) {
    return core::Result<id<MTLTexture>>{
        .error = strdup("unsupported requested pixel format"), .ok = false};
  }
  for (uint32_t i = 0; i < 6; i++) {
    int w, h;
    unsigned char *imageData =
        stbi_load(urls[i], &w, &h, nullptr, desiredChannels);
    if (!imageData) {
      for (uint32_t j = 0; j < i; j++) {
        stbi_image_free(imageDatas[j]);
      }
      [texture release];
      return core::Result<id<MTLTexture>>{
          .error = strdup("failed to load image face"), .ok = false};
    }

    if (w != width || h != height) {
      for (uint32_t j = 0; j < i + 1; j++) {
        stbi_image_free(imageDatas[j]);
      }
      [texture release];
      return core::Result<id<MTLTexture>>{
          .error = strdup("cubemap image faces dimension mismatch"),
          .ok = false};
    }
    imageDatas[i] = imageData;
    if (storageMode == MTLStorageModeShared) {
      MTLRegion region = {{0, 0, 0}, {(uint32_t)width, (uint32_t)height, 1}};
      [texture replaceRegion:region
                 mipmapLevel:0
                       slice:i
                   withBytes:imageData
                 bytesPerRow:width * desiredChannels
               bytesPerImage:width * height * desiredChannels];

      stbi_image_free(imageData);
    }
  }
  if (storageMode == MTLStorageModePrivate) {
    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

    id<MTLBlitCommandEncoder> blitCmdEnc = [commandBuffer blitCommandEncoder];

    for (uint32_t i = 0; i < 6; i++) {
      id<MTLBuffer> stagingBuffer =
          [device newBufferWithBytes:imageDatas[i]
                              length:width * height * desiredChannels
                             options:MTLResourceStorageModeShared];
      free(imageDatas[i]);
      [blitCmdEnc copyFromBuffer:stagingBuffer
                    sourceOffset:0
               sourceBytesPerRow:width * desiredChannels
             sourceBytesPerImage:width * height * desiredChannels
                      sourceSize:MTLSizeMake(width, height, 1)
                       toTexture:texture
                destinationSlice:i
                destinationLevel:0
               destinationOrigin:MTLOriginMake(0, 0, 0)];
      [stagingBuffer release];
    }
    [blitCmdEnc endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    [commandBuffer release];
    [commandQueue release];
  }
  return core::Result<id<MTLTexture>>{.value = texture, .ok = true};
}

} // namespace gal
  //
