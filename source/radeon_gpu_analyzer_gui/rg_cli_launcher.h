#ifndef RGA_RADEONGPUANALYZERGUI_INCLUDE_RG_CLI_LAUNCHER_H_
#define RGA_RADEONGPUANALYZERGUI_INCLUDE_RG_CLI_LAUNCHER_H_

// C++.
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Local.
#include "radeon_gpu_analyzer_gui/rg_data_types.h"

// Forward declarations.
struct RgCliBuildOutput;
struct RgProject;

class RgCliLauncher
{
public:
    // Runs RGA CLI to compile the given Offline OpenCL project clone.
    // project is the project containing the clone to be built.
    // clone_index is the index of the clone to be built.
    // outputPath is where the output files will be generated.
    // cliOutputHandlingCallback is a callback used to send CLI output text to the GUI.
    // cancelSignal can be used to terminate the operation.
    // Returns true for success, false otherwise.
    static bool BuildProjectCloneOpencl(std::shared_ptr<RgProject> project, int clone_index, const std::string& output_path, const std::string& binary_name,
        std::function<void(const std::string&)> cli_output_handling_callback, std::vector<std::string>& gpus_built, bool& cancel_signal);

    // Runs RGA CLI to compile the given Vulkan pipeline project clone.
    // project is the project containing the clone to be built.
    // clone_index is the index of the clone to be built.
    // outputPath is where the output files will be generated.
    // cliOutputHandlingCallback is a callback used to send CLI output text to the GUI.
    // cancelSignal can be used to terminate the operation.
    // Returns true for success, false otherwise.
    static bool BuildProjectCloneVulkan(std::shared_ptr<RgProject> project, int clone_index, const std::string& output_path, const std::string& binary_name,
        std::function<void(const std::string&)> cli_output_handling_callback, std::vector<std::string>& gpus_built, bool& cancel_signal);

    // Runs RGA CLI to disassemble the given SPIR-V binary into text.
    // compilerBinFolder is the folder contaning alternative compiler binaries. If empty, the default spirv-dis tool will be used.
    // spvFullFilePath is the input file path to the SPIR-V binary to disassemble.
    // outputFilePath is the output file path where disassembly text will be dumped.
    // cliOutput is the output message generated by CLI.
    static bool DisassembleSpvToText(const std::string& compiler_bin_folder, const std::string& spv_full_file_path,
                                     const std::string& output_file_path, std::string& cli_output);

    // Runs RGA to generate a version info file for the CLI.
    // full_path is the path to where the version info XML file will be saved to.
    // Returns true for success, false otherwise.
    static bool GenerateVersionInfoFile(const std::string& full_path);

    // Runs RGA to retrieve the start line number for each kernel.
    // project is a pointer to the project being built.
    // clone_index is the index of the clone to access.
    // entrypointLineNumbers A map that gets filled up with the start line numbers for each input file's entrypoints.
    static bool ListKernels(std::shared_ptr<RgProject> project, int clone_index, std::map<std::string, EntryToSourceLineRange>& entrypoint_line_numbers);
private:
    RgCliLauncher() = default;
    ~RgCliLauncher() = default;
};
#endif // RGA_RADEONGPUANALYZERGUI_INCLUDE_RG_CLI_LAUNCHER_H_