#include <metal_stdlib>
using namespace metal;

struct chunkPayload {
  uint resolution;
  uint width, height;
}

struct TerrainData {
  uint32_t width;
  uint32_t height;
  float heightMultiplier;
  float normalStrength;
};

uint distanceFunction(float distance) {
  return 0;
}

struct VertexData    { float4 position [[position]]; };
struct PrimitiveData { float4 color; };

using triangle_mesh_t = metal::mesh<
                                    VertexData,               // Vertex type
                                    PrimitiveData,            // Primitive type
                                    16 * 16,                       // Maximum vertices
                                    15 * 15 * 2,                        // Maximum primitives
                                    metal::topology::triangle // Topology
>;

[[object]]
void objShader(device TerrainData* terrainData [[buffer(0)]], object_data chunkPayload* [[payload]], uint threadID [[thread_index_in_threadgroup]], uint gridID [[threadgroup_position_in_grid]], mesh_grid_properties mgp) {
  if(threadID == 0) {
    mgp.set_threadgroups_per_grid(uint3((terrainData->width + 2) / 8, (terrainData->height + 2) / 8, 1))
  }
  chunkPayload[threadID] = chunkPayload {1, 2, 2};
}

[[mesh]]
void meshShader(triangle_mesh_t outputMesh, uint2 tid [[thread_index_in_threadgroup]]) {
 object_data chunkPayload* [[payload]];
 float4 vertexPosition = float4(tid.x * 8, 0, tid.y * 8, 1.0f);
}
