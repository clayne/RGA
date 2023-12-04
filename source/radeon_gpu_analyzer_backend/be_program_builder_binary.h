//=================================================================
// Copyright 2023 Advanced Micro Devices, Inc. All rights reserved.
//=================================================================

#ifndef RGA_RADEONGPUANALYZERBACKEND_SRC_BE_PROGRAM_BUILDER_BINARY_H_
#define RGA_RADEONGPUANALYZERBACKEND_SRC_BE_PROGRAM_BUILDER_BINARY_H_

// C++.
#include <string>
#include <sstream>
#include <unordered_map>

// Infra.
#include "external/amdt_base_tools/Include/gtString.h"

// Local.
#include "radeon_gpu_analyzer_backend/be_data_types.h"
#include "radeon_gpu_analyzer_backend/be_program_builder.h"
#include "radeon_gpu_analyzer_backend/be_program_builder_vulkan.h"

class beProgramBuilderBinary : public BeProgramBuilder
{
public:
    beProgramBuilderBinary()  = default;
    ~beProgramBuilderBinary() = default;

    // The type of Graphics Api.
    enum class ApiEnum
    {
        // Unknown.
        kUnknown,

        // DXR
        kDXR,

        // Dx12.
        kDX12,

        // Vulkan.
        kVulkan,

        // OpenGL.
        kOpenGL,

        // The count of known Graphics APIs.
        kApiCount
    };

    // return true if the api is a graphics api.
    static bool IsGraphicsApi(ApiEnum api);

    // Gets the api mode from the string version.
    static ApiEnum GetApiFromStr(const std::string& api_str);

    // Gets the string version of the Api mode.
    static std::string GetStrFromApi(ApiEnum api);

    // Gets the Suffixes for stage-specific output files for given Api mode.
    static BeVkPipelineFiles GetStageFileSuffixesFromApi(ApiEnum api);

    // Gets the api mode from the amdpal pipeline metadata.
    static ApiEnum GetApiFromPipelineMetadata(const BeAmdPalMetaData::PipelineMetaData& pipeline);

};
#endif  // RGA_RADEONGPUANALYZERBACKEND_SRC_BE_PROGRAM_BUILDER_BINARY_H_