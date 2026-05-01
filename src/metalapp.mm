#include "Resources/Texture.h"
#include "Resources/argumentTable.h"
#include "Resources/buffer.h"
#include "Resources/command.h"
#include "Resources/depthStencileState.h"
#include "Resources/mtlLibrary.h"
#include "Resources/pipelineState.h"
#include "Resources/residencySet.h"
#include "core/containers.hpp"
#include "core/log.hpp"
#include "gal/gal.hpp"
#include "png-parser/image-processor.h"
#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>

struct LightData {
  glm::vec4 directionalLight;
  glm::vec4 ambientStrength;
  glm::vec4 lightColor;
};

namespace gal {

void cleanup();

core::stack<id, 256> cleanupStack;

bool bad_state = false;

CAMetalLayer *layer;

id<MTLDevice> device;

id<MTL4CommandQueue> commandQueue;

id<MTL4CommandBuffer> commandBuffer;

id<MTL4CommandAllocator> commandAllocator;

id<MTL4ArgumentTable> table;

id<MTLResidencySet> residencySet;

id<MTLRenderPipelineState> pipelineState;

id<MTLRenderPipelineState> skyboxRenderPipelineState;

id<MTLTexture> msaaTexture;

id<MTLSharedEvent> sharedEvent;

id<MTLDepthStencilState> depthStencileState;

id<MTLDepthStencilState> skyboxDepthStencileState;

MTL4RenderPassDescriptor *renderPassDescriptor;

LightData *lightingData;

Camera cam(nullptr, 0.1f, 0.05f, 800, 600);

template <typename T> T checkResult(core::Result<T> result) {
  if (!result.ok) [[unlikely]] {
    cleanup();
    bad_state = true;
    core::log(core::LOG_FATAL, "%s", result.error);
    free((void *)result.error);
    abort();
  }
  return result.value;
}

void getMetalLayer(void *metalLayer) {
  layer = (CAMetalLayer *)metalLayer;
  cam.screenWidth = layer.drawableSize.width;
  cam.screenHeight = layer.drawableSize.height;
  [commandQueue addResidencySet:layer.residencySet];

  TextureCreateInfo msaaTextureCreateInfo{};
  msaaTextureCreateInfo.device = device;
  msaaTextureCreateInfo.storageMode = MTLStorageModePrivate;
  msaaTextureCreateInfo.textureType = MTLTextureType2DMultisample;
  msaaTextureCreateInfo.pixelFormat = layer.pixelFormat;
  msaaTextureCreateInfo.height = layer.drawableSize.height;
  msaaTextureCreateInfo.width = layer.drawableSize.width;
  msaaTextureCreateInfo.sampleCount = 4;
  msaaTexture = checkResult(createTexture(msaaTextureCreateInfo));

  renderPassDescriptor.colorAttachments[0].texture = msaaTexture;
  /* [residencySet addAllocation:msaaTexture]; */
};

void updateLayerSize(uint32_t width, uint32_t height) {
  [layer setDrawableSize:CGSize(width, height)];
  [msaaTexture release];
  TextureCreateInfo msaaTextureCreateInfo{};
  msaaTextureCreateInfo.device = device;
  msaaTextureCreateInfo.storageMode = MTLStorageModePrivate;
  msaaTextureCreateInfo.textureType = MTLTextureType2DMultisample;
  msaaTextureCreateInfo.pixelFormat = layer.pixelFormat;
  msaaTextureCreateInfo.height = layer.drawableSize.height;
  msaaTextureCreateInfo.width = layer.drawableSize.width;
  msaaTextureCreateInfo.sampleCount = 4;
  msaaTexture = checkResult(createTexture(msaaTextureCreateInfo));
  renderPassDescriptor.colorAttachments[0].texture = msaaTexture;
}

struct TerrainData {
  uint vertexWidth;
  uint vertexHeight;
  uint resolution;
  float invResolution;
  float heightMultiplier;
  float normalStrength;
};

TerrainData *terrData = nullptr;

void init() {
  device = MTLCreateSystemDefaultDevice();
  cleanupStack.insert(device);
  commandQueue = [device newMTL4CommandQueue];
  cleanupStack.insert(commandQueue);
  commandBuffer = [device newCommandBuffer];
  cleanupStack.insert(commandBuffer);

  ArgumentTableCreateInfo argumentTableCreateInfo{};
  argumentTableCreateInfo.device = device;
  table = checkResult(createArgumentTable(argumentTableCreateInfo));
  cleanupStack.insert(table);

  ResidencySetCreateInfo residencySetCreateInfo{};
  residencySetCreateInfo.device = device;
  residencySet = checkResult(createResidencySet(residencySetCreateInfo));
  cleanupStack.insert(residencySet);
  [commandQueue addResidencySet:residencySet];

  LibraryCreateInfo libraryCreateInfo{};
  libraryCreateInfo.device = device;
  libraryCreateInfo.URL = "Shaders/build/shader.metallib";
  id<MTLLibrary> library = checkResult(loadMetalLibrary(libraryCreateInfo));

  RenderPipelineStateCreateInfo renderPipelineStateCreateInfo{};
  renderPipelineStateCreateInfo.device = device;
  renderPipelineStateCreateInfo.metalLibrary = library;
  renderPipelineStateCreateInfo.pixelFormat = MTLPixelFormatBGRA8Unorm;
  renderPipelineStateCreateInfo.rasterSampleCount = 4;
  pipelineState =
      checkResult(createRenderPipelineState(renderPipelineStateCreateInfo));
  cleanupStack.insert(pipelineState);

  [library release];

  LibraryCreateInfo skyboxLibraryCreateInfo{};
  skyboxLibraryCreateInfo.device = device;
  skyboxLibraryCreateInfo.URL = "Shaders/build/skybox.metallib";
  id<MTLLibrary> skyboxLibrary =
      checkResult(loadMetalLibrary(skyboxLibraryCreateInfo));

  RenderPipelineStateCreateInfo skyboxRenderPipelineStateCreateInfo{};
  skyboxRenderPipelineStateCreateInfo.device = device;
  skyboxRenderPipelineStateCreateInfo.metalLibrary = skyboxLibrary;
  skyboxRenderPipelineStateCreateInfo.pixelFormat = MTLPixelFormatBGRA8Unorm;
  skyboxRenderPipelineState = checkResult(
      createRenderPipelineState(skyboxRenderPipelineStateCreateInfo));
  cleanupStack.insert(skyboxRenderPipelineState);

  [skyboxLibrary release];

  CommandAllocatorCreateInfo commandAllocatorCreateInfo{};
  commandAllocatorCreateInfo.device = device;
  commandAllocator =
      checkResult(createCommandAllocator(commandAllocatorCreateInfo));
  cleanupStack.insert(commandAllocator);

  sharedEvent = [device newSharedEvent];
  sharedEvent.signaledValue = 1;
  cleanupStack.insert(sharedEvent);

  id<MTLTexture> heightmapTexture = checkResult(loadTextureFromFile(
      "Textures/planes/Rugged Terrain Height Map EXR.exr",
      MTLPixelFormatR32Float, MTLStorageModePrivate, device));
  cleanupStack.insert(heightmapTexture);

  id<MTLTexture> diffuseTexture = checkResult(loadTextureFromFile(
      "Textures/planes/Rugged Terrain Diffuse PNG.png",
      MTLPixelFormatRGBA8Unorm, MTLStorageModePrivate, device));
  cleanupStack.insert(diffuseTexture);

  TerrainData terrainData{512, 512, 1, 1.0f, 256, 1.0f};

  BufferCreateInfo terrainDataBufferCreateInfo{};
  terrainDataBufferCreateInfo.device = device;
  terrainDataBufferCreateInfo.storageMode = MODE_SHARED;
  terrainDataBufferCreateInfo.length = sizeof(terrainData);
  terrainDataBufferCreateInfo.data = &terrainData;
  id<MTLBuffer> terrainDataBuffer =
      checkResult(createBuffer(terrainDataBufferCreateInfo));
  cleanupStack.insert(terrainDataBuffer);
  terrData = (TerrainData *)mapBuffer(terrainDataBuffer).unwrap();
  terrainScale = &terrData->heightMultiplier;
  normalStrength = &terrData->normalStrength;

  BufferCreateInfo bufferCreateInfo{};
  bufferCreateInfo.device = device;
  bufferCreateInfo.storageMode = MODE_SHARED;
  bufferCreateInfo.length = 3 * sizeof(glm::mat4);
  bufferCreateInfo.data = nullptr;
  id<MTLBuffer> uniformBuffer = checkResult(createBuffer(bufferCreateInfo));
  cleanupStack.insert(uniformBuffer);

  LightData light{};
  light.directionalLight = glm::normalize(glm::vec4(0.5f, 1.0f, 0.3f, 1.0f));
  light.ambientStrength = glm::vec4(0.15f, 0.15f, 0.15f, 1.0f);
  light.lightColor = glm::vec4(1.0f, 0.95f, 0.8f, 1.0f); // warm sunlight

  BufferCreateInfo lightDataBufferCreateInfo{};
  lightDataBufferCreateInfo.device = device;
  lightDataBufferCreateInfo.storageMode = MODE_SHARED;
  lightDataBufferCreateInfo.length = sizeof(light);
  lightDataBufferCreateInfo.data = &light;
  id<MTLBuffer> lightDataBuffer =
      checkResult(createBuffer(lightDataBufferCreateInfo));
  lightingData = (LightData *)mapBuffer(lightDataBuffer).unwrap();
  cleanupStack.insert(lightDataBuffer);

  const char *faces[6] = {
      "Textures/cubemap/posx.jpg", "Textures/cubemap/negx.jpg",
      "Textures/cubemap/posy.jpg", "textures/cubemap/negy.jpg",
      "Textures/cubemap/posz.jpg", "Textures/cubemap/negz.jpg"};

  id<MTLTexture> skyboxTexture = checkResult(
      loadCubemapTextureFromFiles(faces, 2048, 2048, MTLPixelFormatRGBA8Unorm,
                                  MTLStorageModeShared, device));

  id<MTLAllocation> allocations[] = {terrainDataBuffer, uniformBuffer,
                                     lightDataBuffer,   heightmapTexture,
                                     diffuseTexture,    skyboxTexture};
  [residencySet addAllocations:allocations count:6];
  [residencySet commit];

  [table setAddress:terrainDataBuffer.gpuAddress atIndex:0];
  [table setTexture:diffuseTexture.gpuResourceID atIndex:2];
  [table setTexture:heightmapTexture.gpuResourceID atIndex:3];
  [table setTexture:skyboxTexture.gpuResourceID atIndex:4];
  [table setAddress:lightDataBuffer.gpuAddress atIndex:5];
  [table setAddress:uniformBuffer.gpuAddress atIndex:1];

  glm::mat4 *matricesPtr = (glm::mat4 *)mapBuffer(uniformBuffer).unwrap();
  *matricesPtr = glm::mat4(1.0f);
  cam.matricesBuffer = matricesPtr + 1;

  DepthStencileStateCreateInfo depthStencileStateCreateInfo{};
  depthStencileStateCreateInfo.device = device;
  depthStencileState =
      checkResult(createDepthStencileState(depthStencileStateCreateInfo));
  cleanupStack.insert(depthStencileState);

  DepthStencileStateCreateInfo skyboxDepthStencileStateCreateInfo{};
  skyboxDepthStencileStateCreateInfo.device = device;
  skyboxDepthStencileStateCreateInfo.depthWriteEnabled = false;
  skyboxDepthStencileState =
      checkResult(createDepthStencileState(skyboxDepthStencileStateCreateInfo));
  cleanupStack.insert(skyboxDepthStencileState);

  renderPassDescriptor = [MTL4RenderPassDescriptor new];
  renderPassDescriptor.colorAttachments[0].clearColor =
      MTLClearColor(0.1, 0.1, 0.1, 1.0);
  renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
  renderPassDescriptor.colorAttachments[0].storeAction =
      MTLStoreActionMultisampleResolve;
  cleanupStack.insert(renderPassDescriptor);
}

void *getMetalDevice() { return device; }

void updateTerrain(uint32_t resolution) {
  if (terrData) {
    terrData->resolution = resolution;
    terrData->invResolution = 1.0f / resolution;
  }
}

void cleanup() {
  [msaaTexture release];
  while (cleanupStack.size()) {
    [*cleanupStack.top().unwrap() release];
    cleanupStack.pop();
  }
}

void resetState() { bad_state = false; }

uint64_t frameNumber = 0;

void rotateVector(const glm::vec3 &axisVector, glm::vec4 *vector, float angle) {
  glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), angle, axisVector);

  *vector = rotationMat * (*vector);

  vector->w = 1.0f;
}

void run(bool wireframe, float pitch, float yaw, int forward, int right,
         int up) {
  @autoreleasepool {
    if (bad_state) [[unlikely]] {
      return;
    }

    if (frameNumber > 0) {
      [sharedEvent waitUntilSignaledValue:frameNumber timeoutMS:UINT64_MAX];
    }

    cam.update(pitch, yaw, forward, right, up);

    id<CAMetalDrawable> drawable = [layer nextDrawable];

    if (drawable == nil) {
      bad_state = true;
      return;
    }

    frameNumber++;

    [commandAllocator reset];

    [commandBuffer beginCommandBufferWithAllocator:commandAllocator];

    id<MTL4RenderCommandEncoder> commandEncoder;
    renderPassDescriptor.colorAttachments[0].resolveTexture =
        [drawable texture];

    commandEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

    [commandEncoder
        setArgumentTable:table
                atStages:MTLRenderStageVertex | MTLRenderStageFragment];

    MTLViewport viewport{};
    viewport.width = layer.drawableSize.width;
    viewport.height = layer.drawableSize.height;
    viewport.zfar = 1.0;
    viewport.znear = 0;

    [commandEncoder setViewport:viewport];

    [commandEncoder setDepthStencilState:skyboxDepthStencileState];

    [commandEncoder setRenderPipelineState:skyboxRenderPipelineState];

    [commandEncoder setCullMode:MTLCullModeBack]; // Damn

    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:0
                       vertexCount:36];

    [commandEncoder setRenderPipelineState:pipelineState];
    [commandEncoder setDepthStencilState:depthStencileState];

    if (wireframe) {
      [commandEncoder setTriangleFillMode:MTLTriangleFillModeLines];
    }

    uint32_t cols = terrData->vertexWidth * terrData->resolution;
    uint32_t rows = terrData->vertexHeight * terrData->resolution;
    uint32_t vertexCount = (cols - 1) * (rows - 1) * 6;

    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                       vertexStart:0
                       vertexCount:vertexCount];

    [commandEncoder endEncoding];

    [commandBuffer endCommandBuffer];

    [commandQueue waitForDrawable:drawable];

    [commandQueue commit:&commandBuffer count:1];

    [commandQueue signalDrawable:drawable];

    [drawable present];

    [commandQueue signalEvent:sharedEvent value:frameNumber];
  }
}

} // namespace gal
