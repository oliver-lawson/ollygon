#pragma once

#include "../core/vec3.hpp"
#include "../core/colour.hpp"
#include "../core/camera.hpp"
#include "render_scene.hpp"
#include "ray.hpp"
#include "optix_types.hpp"

#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
#include <vector>
#include <string>

namespace ollygon {
namespace okaytracer {

// GPU-side params structure
struct OptixParams {
    float* output_buffer;
    int width;
    int height;
    int current_sample;
    int max_bounces;
    uint64_t seed;

    // camera params
    float3 camera_pos;
    float3 viewport_upper_left;
    float3 pixel_delta_u;
    float3 pixel_delta_v;

    // scene data
    OptixTraversableHandle handle;
    GpuRenderPrimitive* primitives;
    int primitive_count;
};

class OptixBackend {
public:
    OptixBackend();
    ~OptixBackend();

    bool initialise();
    void shutdown();

    void build_scene(const RenderScene& scene);

    void render_sample(
        const Camera& camera,
        int width, int height,
        int sample_index,
        int max_bounces,
        uint64_t seed
    );

    void download_pixels(std::vector<float>& pixels);
    bool is_initialised() const { return context != nullptr; }

private:
    void create_context();
    void create_module();
    void create_program_groups();
    void create_pipeline();
    void create_sbt();

    // conversion utilities
    GpuVec3 to_gpu_vec3(const Vec3& v);
    GpuColour to_gpu_colour(const Colour& c);
    GpuMaterial to_gpu_material(const Material& mat);
    GpuRenderPrimitive to_gpu_primitive(const RenderPrimitive& prim);

    OptixDeviceContext context;
    OptixModule module;
    OptixPipeline pipeline;
    OptixShaderBindingTable sbt;

    OptixProgramGroup raygen_prog_group;
    OptixProgramGroup miss_prog_group;
    OptixProgramGroup hitgroup_prog_group;

    OptixPipelineCompileOptions pipeline_compile_options;

    // CUDA resources
    CUdeviceptr d_gas_output_buffer;
    OptixTraversableHandle gas_handle;

    CUdeviceptr d_params;
    CUdeviceptr d_output_buffer;
    CUdeviceptr d_primitives;

    // SBT device memory - need to keep these alive
    CUdeviceptr d_raygen_record;
    CUdeviceptr d_miss_record;
    CUdeviceptr d_hitgroup_record;

    OptixParams params;

    int output_width;
    int output_height;
};

} // namespace okaytracer
} // namespace ollygon
