#include <metal_stdlib>
using namespace metal;

struct RasterizerData {
    float4 position [[position]]; // Clip space position
    float2 textCords;
};

struct TerrainData {
  uint32_t vertexWidth;
  uint32_t vertexHeight;
  uint32_t resolution;
  float invResolution;
  float heightMultiplier;
  float normalStrength;
};

struct sceneMatrices {
  float4x4 model;
  float4x4 view;
  float4x4 projection;
};

struct lightData {
  float4 directionalLight;
  float4 ambientStrength;
  float4 lightColor; 
};

vertex RasterizerData vertexMain(
    uint vid [[vertex_id]],
    constant TerrainData& terrainData [[buffer(0)]],
    constant sceneMatrices& sceneMatrices [[buffer(1)]],
    texture2d<float> heightMap [[texture(3)]])
{
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    uint cols = terrainData.vertexWidth * terrainData.resolution;
    uint rows = terrainData.vertexHeight * terrainData.resolution;
    uint triIndex  = vid / 3;
    uint vertInTri = vid % 3;
    uint quadIndex = triIndex / 2;
    uint triInQuad = triIndex % 2;
    uint quadCol = quadIndex % (cols - 1);
    uint quadRow = quadIndex / (cols - 1);
    const uint2 offsets[2][3] = {
        { {0,0}, {1,0}, {0,1} },
        { {1,0}, {1,1}, {0,1} }
    };
    uint2 offset = offsets[triInQuad][vertInTri];
    uint col = quadCol + offset.x;
    uint row = quadRow + offset.y;
    float u = (float)col / (float)(cols - 1);
    float v = (float)row / (float)(rows - 1);
    float x = u * (float)terrainData.vertexWidth;
    float z = v * (float)terrainData.vertexHeight;
    float y = heightMap.sample(textureSampler, float2(u, v)).r * terrainData.heightMultiplier;
    RasterizerData out;
    out.position = sceneMatrices.projection * sceneMatrices.view * sceneMatrices.model * float4(x, y, z, 1.0f);
    out.textCords = float2(u, v);
    return out;
}

fragment float4 fragmentMain(RasterizerData in [[stage_in]], constant TerrainData& terrainData[[buffer(0)]], texture2d<float> diffuseTexture [[texture(2)]], texture2d<float> heightMap [[texture(3)]], constant lightData& lightingData [[buffer(5)]]) {
  
  constexpr sampler textureSampler(mag_filter::linear, min_filter::linear, address::clamp_to_edge);

  float texelSize = 1.0f / float(heightMap.get_width());
  float scale = terrainData.normalStrength;

  float hL = heightMap.sample(textureSampler, float2(in.textCords.x - texelSize, in.textCords.y)).r * scale;
  float hR = heightMap.sample(textureSampler, float2(in.textCords.x + texelSize, in.textCords.y)).r * scale;
  float hD = heightMap.sample(textureSampler, float2(in.textCords.x, in.textCords.y - texelSize)).r * scale;
  float hU = heightMap.sample(textureSampler, float2(in.textCords.x, in.textCords.y + texelSize)).r * scale;

  float3 normal = normalize(float3(hL - hR, texelSize, hD - hU));

  float3 color = diffuseTexture.sample(textureSampler, in.textCords).rgb;
  
  float3 ambient = lightingData.ambientStrength.xyz * lightingData.lightColor.xyz;
  
  float diff = max(dot(normal, lightingData.directionalLight.xyz), 0.0f);

  float3 diffuse = diff * lightingData.lightColor.xyz;

  return float4((ambient + diffuse) * color, 1.0f);
}
