#include <metal_stdlib>
using namespace metal;

struct RasterizerData {
    float4 position [[position]]; // Clip space position
    float3 direction;
};

struct sceneMatrices {
  float4x4 model;
  float4x4 view;
  float4x4 projection;
};

constant float3 cubeVertices[36] = {
    // -Z
    {-1, -1, -1}, { 1,  1, -1}, { 1, -1, -1},
    {-1, -1, -1}, {-1,  1, -1}, { 1,  1, -1},
    // +Z
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1},
    {-1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
    // -X
    {-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1},
    {-1, -1, -1}, {-1,  1,  1}, {-1,  1, -1},
    // +X
    { 1, -1, -1}, { 1,  1,  1}, { 1, -1,  1},
    { 1, -1, -1}, { 1,  1, -1}, { 1,  1,  1},
    // -Y
    {-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1},
    {-1, -1, -1}, { 1, -1,  1}, {-1, -1,  1},
    // +Y
    {-1,  1, -1}, { 1,  1,  1}, { 1,  1, -1},
    {-1,  1, -1}, {-1,  1,  1}, { 1,  1,  1},
};

float4x4 removeTranslation(float4x4 m) {
    return float4x4(
        float4(m[0].xyz, 0),
        float4(m[1].xyz, 0),
        float4(m[2].xyz, 0),
        float4(0, 0, 0, 1)
    );
}

vertex RasterizerData vertexMain(uint vid [[vertex_id]], constant sceneMatrices& matrices [[buffer(1)]]) {
  float3 position = cubeVertices[vid];  
  RasterizerData out;
  out.position = matrices.projection * removeTranslation(matrices.view) * float4(position, 1.0f);
  out.direction = normalize(position);
  return out; 
}

fragment float4 fragmentMain(RasterizerData in [[stage_in]], texturecube<float> sky [[texture(4)]]) {
  constexpr sampler cubeSampler(filter::linear, mip_filter::linear, address::clamp_to_edge);
  return float4(sky.sample(cubeSampler, in.direction).xyz * 2, 1.0f);
}
