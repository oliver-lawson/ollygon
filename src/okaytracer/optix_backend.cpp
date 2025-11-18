#define NOMINMAX

#include "optix_backend.hpp"
#include "optix_function_table_definition.h"
#include "optix_stack_size.h"
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace ollygon {
namespace okaytracer {

#define OPTIX_CHECK(call) \
    do { \
        OptixResult res = call; \
        if (res != OPTIX_SUCCESS) { \
            const char* error_str = optixGetErrorString(res); \
            std::cerr << "OptiX error: " << (error_str ? error_str : "unknown error") \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while(0)

#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            const char* error_str = cudaGetErrorString(error); \
            std::cerr << "CUDA error: " << (error_str ? error_str : "unknown error") \
                      << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        } \
    } while(0)

OptixBackend::OptixBackend()
    : context(nullptr)
    , module(nullptr)
    , pipeline(nullptr)
    , raygen_prog_group(nullptr)
    , miss_prog_group(nullptr)
    , hitgroup_prog_group(nullptr)
    , d_gas_output_buffer(0)
    , gas_handle(0)
    , d_params(0)
    , d_output_buffer(0)
    , d_primitives(0)
    , d_raygen_record(0)
    , d_miss_record(0)
    , d_hitgroup_record(0)
    , output_width(0)
    , output_height(0)
{
    memset(&pipeline_compile_options, 0, sizeof(OptixPipelineCompileOptions));
}

OptixBackend::~OptixBackend() {
    shutdown();
}

bool OptixBackend::initialise() {
    OptixResult result = optixInit();
    if (result != OPTIX_SUCCESS) {
        std::cerr << "failed to initialise OptiX: " << optixGetErrorString(result) << "\n";
        return false;
    }
    std::cout << "OptiX initialised successfully\n";

    int cuda_device_count = 0;
    cudaError_t cuda_error = cudaGetDeviceCount(&cuda_device_count);

    if (cuda_error != cudaSuccess) {
        std::cerr << "CUDA device count failed: " << cudaGetErrorString(cuda_error) << "\n";
        return false;
    }

    if (cuda_device_count == 0) {
        std::cerr << "no CUDA devices found\n";
        return false;
    }

    std::cout << "found " << cuda_device_count << " CUDA devices\n";

    for (int i = 0; i < cuda_device_count; i++) {
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
            std::cout << "device " << i << ": " << prop.name
                << " (compute " << prop.major << "." << prop.minor << ")\n";
        }
    }

    CUDA_CHECK(cudaSetDevice(0));

    //cudaFree(0) forces CUDA context creation - to verify device actually works
    cuda_error = cudaFree(0);
    if (cuda_error != cudaSuccess) {
        std::cerr << "CUDA context creation failed: " << cudaGetErrorString(cuda_error) << "\n";
        return false;
    }

    std::cout << "CUDA initialised successfully!\n";

    create_context();
    create_module();
    create_program_groups();
    create_pipeline();
    create_sbt();

    return context != nullptr;
}

void OptixBackend::shutdown() {
    if (d_output_buffer) {
        CUDA_CHECK(cudaFree((void*)d_output_buffer));
        d_output_buffer = 0;
    }
    if (d_primitives) {
        CUDA_CHECK(cudaFree((void*)d_primitives));
        d_primitives = 0;
    }
    if (d_gas_output_buffer) {
        CUDA_CHECK(cudaFree((void*)d_gas_output_buffer));
        d_gas_output_buffer = 0;
    }
    if (d_params) {
        CUDA_CHECK(cudaFree((void*)d_params));
        d_params = 0;
    }

    if (pipeline) optixPipelineDestroy(pipeline);
    if (raygen_prog_group) optixProgramGroupDestroy(raygen_prog_group);
    if (miss_prog_group) optixProgramGroupDestroy(miss_prog_group);
    if (hitgroup_prog_group) optixProgramGroupDestroy(hitgroup_prog_group);
    if (module) optixModuleDestroy(module);
    if (context) optixDeviceContextDestroy(context);
    if (d_raygen_record) cudaFree((void*)d_raygen_record);
    if (d_miss_record) cudaFree((void*)d_miss_record);
    if (d_hitgroup_record) cudaFree((void*)d_hitgroup_record);
}

void OptixBackend::create_context() {
    CUcontext cu_ctx = 0;  // use current CUDA context
    OptixDeviceContextOptions options = {};
    options.logCallbackFunction = nullptr;
    options.logCallbackLevel = 4;

    OPTIX_CHECK(optixDeviceContextCreate(cu_ctx, &options, &context));
}

void OptixBackend::create_module() {
    std::ifstream ptx_file("optix_kernels.ptx");
    if (!ptx_file) {
        std::cerr << "failed to load optix_kernels.ptx - file not found!\n";
        return;
    }

    std::stringstream buffer;
    buffer << ptx_file.rdbuf();
    std::string ptx = buffer.str();

    if (ptx.empty()) {
        std::cerr << "PTX file is empty!\n";
        return;
    }

    std::cout << "PTX loaded successfully, size: " << ptx.size() << " bytes\n";

    OptixModuleCompileOptions module_compile_options = {};
    module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
    module_compile_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_LEVEL_3;
    module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;

    pipeline_compile_options.usesMotionBlur = false;
    pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
    // TEMP - 15 payload values needed bc we pass full ray state iteratively for now (until wavefront)
    pipeline_compile_options.numPayloadValues = 15;
    pipeline_compile_options.numAttributeValues = 3;
    pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

    char log[2048];
    size_t log_size = sizeof(log);

    OPTIX_CHECK(optixModuleCreate(
        context,
        &module_compile_options,
        &pipeline_compile_options,
        ptx.c_str(),
        ptx.size(),
        log, &log_size,
        &module
    ));

    if (log_size > 1) {
        std::cout << "OptiX module log:\n" << log << "\n";
    }

    if (!module) {
        std::cerr << "failed to create OptiX module\n";
    }
    else {
        std::cout << "OptiX module created successfully!\n";
    }
}

void OptixBackend::create_program_groups() {
    char log[2048];
    size_t log_size;

    OptixProgramGroupOptions prog_group_options = {};

    // == RayGen ==
    OptixProgramGroupDesc raygen_prog_group_desc = {};
    raygen_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
    raygen_prog_group_desc.raygen.module = module;
    raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__rg";

    log_size = sizeof(log);
    OptixResult result = optixProgramGroupCreate(
        context,
        &raygen_prog_group_desc,
        1,
        &prog_group_options,
        log, &log_size,
        &raygen_prog_group
    );

    if (result != OPTIX_SUCCESS) {
        std::cerr << "failed to create raygen program group: " << optixGetErrorString(result) << "\n";
        if (log_size > 1) std::cerr << "Log: " << log << "\n";
        return;
    }

    // == Miss ==

    OptixProgramGroupDesc miss_prog_group_desc = {};
    miss_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
    miss_prog_group_desc.miss.module = module;
    miss_prog_group_desc.miss.entryFunctionName = "__miss__ms";

    log_size = sizeof(log);
    result = optixProgramGroupCreate(
        context,
        &miss_prog_group_desc,
        1,
        &prog_group_options,
        log, &log_size,
        &miss_prog_group
    );

    if (result != OPTIX_SUCCESS) {
        std::cerr << "failed to create miss program group: " << optixGetErrorString(result) << "\n";
        if (log_size > 1) std::cerr << "Log: " << log << "\n";
        return;
    }

    // == HitGroup ==

    OptixProgramGroupDesc hitgroup_prog_group_desc = {};
    hitgroup_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    hitgroup_prog_group_desc.hitgroup.moduleCH = module;
    hitgroup_prog_group_desc.hitgroup.entryFunctionNameCH = "__closesthit__ch";
    hitgroup_prog_group_desc.hitgroup.moduleIS = module;
    hitgroup_prog_group_desc.hitgroup.entryFunctionNameIS = "__intersection__is";

    log_size = sizeof(log);
    result = optixProgramGroupCreate(
        context,
        &hitgroup_prog_group_desc,
        1,
        &prog_group_options,
        log, &log_size,
        &hitgroup_prog_group
    );

    if (result != OPTIX_SUCCESS) {
        std::cerr << "failed to create hitgroup program group: " << optixGetErrorString(result) << "\n";
        if (log_size > 1) std::cerr << "Log: " << log << "\n";
        return;
    }

    std::cout << "all program groups created successfully!\n";
}

void OptixBackend::create_pipeline()
{
    if (!raygen_prog_group || !miss_prog_group || !hitgroup_prog_group) {
        std::cerr << "missing program groups for pipeline creation..\n";
        return;
    }

    OptixProgramGroup program_groups[] = {
        raygen_prog_group,
        miss_prog_group,
        hitgroup_prog_group
    };

    OptixPipelineLinkOptions pipeline_link_options = {};
    // depth 1 as we do iterative tracing in raygen, not recursive
    pipeline_link_options.maxTraceDepth = 1;

    OptixPipelineCompileOptions pipeline_compile_options = {};
    pipeline_compile_options.usesMotionBlur = false;
    pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
    pipeline_compile_options.numPayloadValues = 15;
    pipeline_compile_options.numAttributeValues = 3;
    pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

    char log[2048];
    size_t log_size = sizeof(log);

    OptixResult result = optixPipelineCreate(
        context,
        &pipeline_compile_options,
        &pipeline_link_options,
        program_groups,
        3,
        log, &log_size,
        &pipeline
    );

    if (result != OPTIX_SUCCESS) {
        std::cerr << "failed to create pipeline: " << optixGetErrorString(result) << "\n";
        if (log_size > 1) std::cerr << "pipeline log: " << log << "\n";
        pipeline = nullptr;
        return;
    }

    std::cout << "pipeline created successfully\n";

    /////
    // stack size determining using optix_stack_size.h
    OptixStackSizes stack_sizes = {};
    memset(&stack_sizes, 0, sizeof(OptixStackSizes));

    //passing nullptr to OptixPipeline as not using external functions yet, this seems to work
    OPTIX_CHECK(optixUtilAccumulateStackSizes(raygen_prog_group, &stack_sizes, nullptr)); 
    OPTIX_CHECK(optixUtilAccumulateStackSizes(miss_prog_group, &stack_sizes, nullptr));
    OPTIX_CHECK(optixUtilAccumulateStackSizes(hitgroup_prog_group, &stack_sizes, nullptr));

    uint32_t max_trace_depth = 1; ///iterative
    uint32_t max_cc_depth = 0;    // no continuation callables
    uint32_t max_dc_depth = 0;    // no direct callables

    uint32_t direct_callable_stack_size_from_traversal;
    uint32_t direct_callable_stack_size_from_state;
    uint32_t continuation_stack_size;

    OPTIX_CHECK(optixUtilComputeStackSizes(
        &stack_sizes,
        max_trace_depth,
        max_cc_depth,
        max_dc_depth,
        &direct_callable_stack_size_from_traversal,
        &direct_callable_stack_size_from_state,
        &continuation_stack_size
    ));

    OPTIX_CHECK(optixPipelineSetStackSize(
        pipeline,
        direct_callable_stack_size_from_traversal,
        direct_callable_stack_size_from_state,
        continuation_stack_size,
        max_trace_depth
    ));
}

void OptixBackend::create_sbt()
{
    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) RaygenRecord {
        char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    };
    RaygenRecord rg_record;
    memset(&rg_record, 0, sizeof(RaygenRecord));
    OPTIX_CHECK(optixSbtRecordPackHeader(raygen_prog_group, &rg_record));
    CUdeviceptr d_raygen_record;
    CUDA_CHECK(cudaMalloc((void**)&d_raygen_record, sizeof(RaygenRecord)));
    CUDA_CHECK(cudaMemcpy(
        (void*)d_raygen_record,
        &rg_record,
        sizeof(RaygenRecord),
        cudaMemcpyHostToDevice
    ));

    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) MissRecord {
        char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    };
    MissRecord ms_record;
    memset(&ms_record, 0, sizeof(MissRecord));
    OPTIX_CHECK(optixSbtRecordPackHeader(miss_prog_group, &ms_record));

    CUdeviceptr d_miss_record;
    CUDA_CHECK(cudaMalloc((void**)&d_miss_record, sizeof(MissRecord)));
    CUDA_CHECK(cudaMemcpy(
        (void*)d_miss_record,
        &ms_record,
        sizeof(MissRecord),
        cudaMemcpyHostToDevice
    ));

    struct __align__(OPTIX_SBT_RECORD_ALIGNMENT) HitGroupRecord {
        char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    };
    HitGroupRecord hg_record;
    memset(&hg_record, 0, sizeof(HitGroupRecord));
    OPTIX_CHECK(optixSbtRecordPackHeader(hitgroup_prog_group, &hg_record));
    CUdeviceptr d_hitgroup_record;
    CUDA_CHECK(cudaMalloc((void**)&d_hitgroup_record, sizeof(HitGroupRecord)));
    CUDA_CHECK(cudaMemcpy(
        (void*)d_hitgroup_record,
        &hg_record,
        sizeof(HitGroupRecord),
        cudaMemcpyHostToDevice
    ));

    // critical: we have to zero-init the entire SBT struct first, or optix crashes
    memset(&sbt, 0, sizeof(OptixShaderBindingTable));

    sbt.raygenRecord = d_raygen_record;

    sbt.missRecordBase = d_miss_record;
    sbt.missRecordStrideInBytes = sizeof(MissRecord);
    sbt.missRecordCount = 1;

    sbt.hitgroupRecordBase = d_hitgroup_record;
    sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupRecord);
    sbt.hitgroupRecordCount = 1;

    sbt.exceptionRecord = 0;
    sbt.callablesRecordBase = 0;
    sbt.callablesRecordCount = 0;
    sbt.callablesRecordStrideInBytes = 0;

    std::cout << "SBT setup complete\n";
}

void OptixBackend::build_scene(const RenderScene& scene)
{
    //std::cout << "build_scene() called with " << scene.primitives.size() << " primitives\n";
    
    // clear existing GAS
    if (d_gas_output_buffer) {
        CUDA_CHECK(cudaFree((void*)d_gas_output_buffer));
        d_gas_output_buffer = 0;
        gas_handle = 0;
    }
    // convert to gpu-compatible format
    std::vector<GpuRenderPrimitive> gpu_primitives;
    gpu_primitives.reserve(scene.primitives.size());
    for (const auto& prim : scene.primitives) {
        gpu_primitives.push_back(to_gpu_primitive(prim));
    }
    // copy gpu prims to device
    size_t prim_bytes = gpu_primitives.size() * sizeof(GpuRenderPrimitive);
    if (d_primitives) {
        CUDA_CHECK(cudaFree((void*)d_primitives));
    }
    if (prim_bytes > 0) {
        CUDA_CHECK(cudaMalloc((void**)&d_primitives, prim_bytes));
        CUDA_CHECK(cudaMemcpy(
            (void*)d_primitives,
            gpu_primitives.data(),
            prim_bytes,
            cudaMemcpyHostToDevice
        ));
    }
    params.primitives = (GpuRenderPrimitive*)d_primitives;
    params.primitive_count = static_cast<int>(gpu_primitives.size());

    if (scene.primitives.empty()) {
        params.handle = 0; // empty scene
        return;
    }

    // build custom aabbs for the ollygon prims
    std::vector<OptixAabb> aabbs;
    aabbs.reserve(scene.primitives.size());

    for (const auto& prim : scene.primitives) {
        OptixAabb aabb;

        switch (prim.type) {
        case RenderPrimitive::Type::Sphere: {
            aabb.minX = prim.centre.x - prim.radius;
            aabb.minY = prim.centre.y - prim.radius;
            aabb.minZ = prim.centre.z - prim.radius;
            aabb.maxX = prim.centre.x + prim.radius;
            aabb.maxY = prim.centre.y + prim.radius;
            aabb.maxZ = prim.centre.z + prim.radius;
            break;
        }
        case RenderPrimitive::Type::Quad: {
            Vec3 corners[4] = {
                prim.quad_corner,
                prim.quad_corner + prim.quad_u,
                prim.quad_corner + prim.quad_v,
                prim.quad_corner + prim.quad_u + prim.quad_v
            };

            aabb.minX = aabb.maxX = corners[0].x;
            aabb.minY = aabb.maxY = corners[0].y;
            aabb.minZ = aabb.maxZ = corners[0].z;

            for (int i = 1; i < 4; i++) {
                aabb.minX = std::fmin(aabb.minX, corners[i].x);
                aabb.minY = std::fmin(aabb.minY, corners[i].y);
                aabb.minZ = std::fmin(aabb.minZ, corners[i].z);
                aabb.maxX = std::fmax(aabb.maxX, corners[i].x);
                aabb.maxY = std::fmax(aabb.maxY, corners[i].y);
                aabb.maxZ = std::fmax(aabb.maxZ, corners[i].z);
            }
            break;
        }
        case RenderPrimitive::Type::Triangle: {
            // std::fmin/fmax only take 2 args so have to nest them
            aabb.minX = std::fmin(prim.tri_v0.x, std::fmin(prim.tri_v1.x, prim.tri_v2.x));
            aabb.minY = std::fmin(prim.tri_v0.y, std::fmin(prim.tri_v1.y, prim.tri_v2.y));
            aabb.minZ = std::fmin(prim.tri_v0.z, std::fmin(prim.tri_v1.z, prim.tri_v2.z));
            aabb.maxX = std::fmax(prim.tri_v0.x, std::fmax(prim.tri_v1.x, prim.tri_v2.x));
            aabb.maxY = std::fmax(prim.tri_v0.y, std::fmax(prim.tri_v1.y, prim.tri_v2.y));
            aabb.maxZ = std::fmax(prim.tri_v0.z, std::fmax(prim.tri_v1.z, prim.tri_v2.z));
            break;
        }
        }

        aabbs.push_back(aabb);
    }

    // upload the aabbs to gpu
    CUdeviceptr d_aabbs;
    size_t aabb_bytes = aabbs.size() * sizeof(OptixAabb);
    CUDA_CHECK(cudaMalloc((void**)&d_aabbs, aabb_bytes));
    CUDA_CHECK(cudaMemcpy(
        (void*)d_aabbs,
        aabbs.data(),
        aabb_bytes,
        cudaMemcpyHostToDevice
    ));

    // build input, flag (one for all prims), build options
    OptixBuildInput build_input = {};
    build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    build_input.customPrimitiveArray.aabbBuffers = &d_aabbs;
    build_input.customPrimitiveArray.numPrimitives = static_cast<unsigned int>(aabbs.size());
    build_input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
    static uint32_t build_flags = OPTIX_GEOMETRY_FLAG_NONE;
    build_input.customPrimitiveArray.flags = &build_flags;
    build_input.customPrimitiveArray.numSbtRecords = 1;
    build_input.customPrimitiveArray.sbtIndexOffsetBuffer = 0;
    build_input.customPrimitiveArray.sbtIndexOffsetSizeInBytes = 0;
    build_input.customPrimitiveArray.sbtIndexOffsetStrideInBytes = 0;
    OptixAccelBuildOptions accel_options = {};
    // tried w/ compaction but was slower on complex scenes.  TODO revisit compaction for wavefront
    accel_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;

    // calc memory requirements of gas
    OptixAccelBufferSizes gas_buffer_sizes;
    OPTIX_CHECK(optixAccelComputeMemoryUsage(
        context,
        &accel_options,
        &build_input,
        1,
        &gas_buffer_sizes
    ));

    // allocate temp buffer
    CUdeviceptr d_temp_buffer;
    CUDA_CHECK(cudaMalloc((void**)&d_temp_buffer, gas_buffer_sizes.tempSizeInBytes));
    if (d_gas_output_buffer) {
        CUDA_CHECK(cudaFree((void*)d_gas_output_buffer));
    }
    CUDA_CHECK(cudaMalloc((void**)&d_gas_output_buffer, gas_buffer_sizes.outputSizeInBytes));
   
    // finally - build the GAS
    OPTIX_CHECK(optixAccelBuild(
        context,
        0,
        &accel_options,
        &build_input,
        1,
        d_temp_buffer,
        gas_buffer_sizes.tempSizeInBytes,
        d_gas_output_buffer,
        gas_buffer_sizes.outputSizeInBytes,
        &gas_handle,
        nullptr,
        0
    ));

    // free temp buffer
    CUDA_CHECK(cudaFree((void*)d_temp_buffer));
    CUDA_CHECK(cudaFree((void*)d_aabbs));

    params.handle = gas_handle;

    std::cout << "GAS built with " << scene.primitives.size() << " primitives\n";
}


void OptixBackend::render_sample(const Camera& camera, int width, int height, int sample_index, int max_bounces, uint64_t seed)
{
    if (!pipeline) {
        std::cerr << "pipeline not created - cannot render\n";
        return;
    }
    // allocate output buffer if needed
    if (width != output_width || height != output_height || !d_output_buffer) {
        if (d_output_buffer) {
            CUDA_CHECK(cudaFree((void*)d_output_buffer));
        }

        size_t buffer_size = width * height * 3 * sizeof(float);
        CUDA_CHECK(cudaMalloc((void**)&d_output_buffer, buffer_size));
        CUDA_CHECK(cudaMemset((void*)d_output_buffer, 0, buffer_size));

        output_width = width;
        output_height = height;
    }

    // update params
    params.output_buffer = (float*)d_output_buffer;
    params.width = width;
    params.height = height;
    params.current_sample = sample_index;
    params.max_bounces = max_bounces;
    params.seed = seed;

    // camera basis
    Vec3 forward = (camera.get_target() - camera.get_pos()).normalised();
    Vec3 right = Vec3::cross(forward, camera.get_up()).normalised();
    Vec3 up = Vec3::cross(right, forward);

    float aspect = float(width) / float(height);
    float fov_rad = camera.get_fov_degs() * DEG_TO_RAD;
    float h = std::tan(fov_rad * 0.5f);
    float viewport_height = 2.0f * h;
    float viewport_width = viewport_height * aspect;

    Vec3 viewport_u = right * viewport_width;
    Vec3 viewport_v = up * viewport_height;

    Vec3 viewport_upper_left = camera.get_pos() + forward - viewport_u * 0.5f + viewport_v * 0.5f;
    Vec3 pixel_delta_u = viewport_u / float(width);
    Vec3 pixel_delta_v = viewport_v / float(height);
    Vec3 camera_pos = camera.get_pos();

    params.viewport_upper_left = make_float3(viewport_upper_left.x, viewport_upper_left.y, viewport_upper_left.z);
    params.pixel_delta_u = make_float3(pixel_delta_u.x, pixel_delta_u.y, pixel_delta_u.z);
    params.pixel_delta_v = make_float3(pixel_delta_v.x, pixel_delta_v.y, pixel_delta_v.z);
    params.camera_pos = make_float3(camera_pos.x, camera_pos.y, camera_pos.z);

    // upload params
    if (!d_params) {
        CUDA_CHECK(cudaMalloc((void**)&d_params, sizeof(OptixParams)));
    }

    CUDA_CHECK(cudaMemcpy(
        (void*)d_params,
        &params,
        sizeof(OptixParams),
        cudaMemcpyHostToDevice
    ));

    OPTIX_CHECK(optixLaunch(
        pipeline,
        0,
        d_params,
        sizeof(OptixParams),
        &sbt,
        width,
        height,
        1
    ));

    CUDA_CHECK(cudaDeviceSynchronize());
}

void OptixBackend::download_pixels(std::vector<float>& pixels)
{
    if (!d_output_buffer || output_width == 0 || output_height == 0) {
        pixels.clear();
        return;
    }
    size_t buffer_size = output_width * output_height * 3 * sizeof(float);
    pixels.resize(output_width * output_height * 3);

    CUDA_CHECK(cudaMemcpy(
        pixels.data(),
        (void*)d_output_buffer,
        buffer_size,
        cudaMemcpyDeviceToHost
    ));
}

// == type conversions ==

GpuVec3 OptixBackend::to_gpu_vec3(const Vec3& v) {
    return GpuVec3{ v.x, v.y, v.z };
}

GpuColour OptixBackend::to_gpu_colour(const Colour& c) {
    return GpuColour{ c.r, c.g, c.b };
}

GpuMaterial OptixBackend::to_gpu_material(const Material& mat) {
    GpuMaterial gpu_mat;

    switch (mat.type) {
    case MaterialType::Lambertian:
        gpu_mat.type = GpuMaterialType::Lambertian;
        break;
    case MaterialType::Metal:
        gpu_mat.type = GpuMaterialType::Metal;
        break;
    case MaterialType::Dielectric:
        gpu_mat.type = GpuMaterialType::Dielectric;
        break;
    case MaterialType::Emissive:
        gpu_mat.type = GpuMaterialType::Emissive;
        break;
    case MaterialType::Chequerboard:
        gpu_mat.type = GpuMaterialType::Chequerboard;
        break;
    }

    gpu_mat.albedo = to_gpu_colour(mat.albedo);
    gpu_mat.emission = to_gpu_colour(mat.emission);
    gpu_mat.roughness = mat.roughness;
    gpu_mat.ior = mat.ior;
    gpu_mat.chequerboard_colour_a = to_gpu_colour(mat.chequerboard_colour_a);
    gpu_mat.chequerboard_colour_b = to_gpu_colour(mat.chequerboard_colour_b);
    gpu_mat.chequerboard_scale = mat.chequerboard_scale;

    return gpu_mat;
}

GpuRenderPrimitive OptixBackend::to_gpu_primitive(const RenderPrimitive& prim) {
    GpuRenderPrimitive gpu_prim;

    switch (prim.type) {
    case RenderPrimitive::Type::Sphere:
        gpu_prim.type = GpuPrimitiveType::Sphere;
        break;
    case RenderPrimitive::Type::Quad:
        gpu_prim.type = GpuPrimitiveType::Quad;
        break;
    case RenderPrimitive::Type::Triangle:
        gpu_prim.type = GpuPrimitiveType::Triangle;
        break;
    }

    gpu_prim.centre = to_gpu_vec3(prim.centre);
    gpu_prim.radius = prim.radius;
    gpu_prim.quad_corner = to_gpu_vec3(prim.quad_corner);
    gpu_prim.quad_u = to_gpu_vec3(prim.quad_u);
    gpu_prim.quad_v = to_gpu_vec3(prim.quad_v);
    gpu_prim.quad_normal = to_gpu_vec3(prim.quad_normal);
    gpu_prim.tri_v0 = to_gpu_vec3(prim.tri_v0);
    gpu_prim.tri_v1 = to_gpu_vec3(prim.tri_v1);
    gpu_prim.tri_v2 = to_gpu_vec3(prim.tri_v2);
    gpu_prim.tri_n0 = to_gpu_vec3(prim.tri_n0);
    gpu_prim.tri_n1 = to_gpu_vec3(prim.tri_n1);
    gpu_prim.tri_n2 = to_gpu_vec3(prim.tri_n2);
    gpu_prim.material = to_gpu_material(prim.material);

    return gpu_prim;
}

} // namespace okaytracer
} // namespace ollygon
