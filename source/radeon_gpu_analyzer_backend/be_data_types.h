//=============================================================================
/// Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for rga backend dtaa types.
//=============================================================================

#ifndef RGA_RADEONGPUANALYZERBACKEND_SRC_BE_DATA_TYPES_H_
#define RGA_RADEONGPUANALYZERBACKEND_SRC_BE_DATA_TYPES_H_

#include <array>
#include <vector>
#include <string>

#include <external/amdt_base_tools/Include/gtString.h>

// A structure to hold the shader file names of a given pipeline.
struct BeProgramPipeline
{
    // Clears all pipeline shaders.
    void ClearAll()
    {
        vertex_shader.makeEmpty();
        tessellation_control_shader.makeEmpty();
        tessellation_evaluation_shader.makeEmpty();
        geometry_shader.makeEmpty();
        fragment_shader.makeEmpty();
        compute_shader.makeEmpty();
        mesh_shader.makeEmpty();
        task_shader.makeEmpty();
    }

    // Vertex shader.
    gtString vertex_shader;

    // Tessellation control shader.
    gtString tessellation_control_shader;

    // Tessellation evaluation shader.
    gtString tessellation_evaluation_shader;

    // Geometry shader.
    gtString geometry_shader;

    // Fragment shader.
    gtString fragment_shader;

    // Compute shader.
    gtString compute_shader;

    // Mesh shader.
    gtString mesh_shader;

    // Task shader.
    gtString task_shader;
};

// The type of stage used for a shader module.
enum BePipelineStage : char
{
    kVertex,
    kTessellationControl,
    kTessellationEvaluation,
    kGeometry,
    kFragment,
    kCompute,
    kMesh,
    kTask,

    kCount
};

// An array containing per-stage file names.
typedef std::array<std::string, BePipelineStage::kCount>  BeVkPipelineFiles;

// The type of stage used for a shader module.
enum BeRtxPipelineStage : char
{
    kRayGeneration,
    kIntersection,
    kAnyHit,
    kClosestHit,
    kMiss,
    kCallable,
    kTraversal,
    kLaunchKernel,

    kCountRtx
};

// An array containing per-stage file names.
typedef std::array<std::string, BeRtxPipelineStage::kCountRtx> BeRtxPipelineFiles;

// Ray Tracing Stage Name strings.
static const BeRtxPipelineFiles kStrRtxStageNames =
{
    "RayGeneration",
    "Intersection",
    "AnyHit",
    "ClosestHit",
    "Miss",
    "Callable",  
    "Traversal", 
    "LaunchKernel"
};

// Rtx Suffixes for stage-specific output files.
static const BeRtxPipelineFiles kStrRtxStageSuffix = kStrRtxStageNames;

// Dx12 Stage Name strings.
static const BeVkPipelineFiles kStrDx12StageNames =
{
    "vertex",
    "hull",
    "domain",
    "geometry",
    "pixel",
    "compute", 
    "mesh", 
    "task"
};

// DX12 Suffixes for stage-specific output files.
static const BeVkPipelineFiles kStrDx12StageSuffix =
{
    "vert",
    "hull",
    "domain",
    "geom",
    "pixel",
    "comp", 
    "mesh", 
    "task"
};

// Vulkan Suffixes for stage-specific output files.
static const BeVkPipelineFiles kVulkanStageFileSuffix = 
{
    "vert", 
    "tesc", 
    "tese", 
    "geom", 
    "frag", 
    "comp", 
    "mesh", 
    "task"
};

// Physical adapter data.
struct BeVkPhysAdapterInfo
{
    uint32_t     id;
    std::string  name;
    std::string  vk_driver_version;
    std::string  vk_api_version;
};

enum beWaveSize
{
    kUnknown = 0,
    kWave32,
    kWave64
};

// An array containing per-stage wave size.
typedef std::array<beWaveSize, BePipelineStage::kCount> BeVkPipelineWaveSizes;

// An array containing per-stage wave size.
typedef std::array<beWaveSize, BeRtxPipelineStage::kCountRtx> BeRtxPipelineWaveSizes;

#endif // RGA_RADEONGPUANALYZERBACKEND_SRC_BE_DATA_TYPES_H_
