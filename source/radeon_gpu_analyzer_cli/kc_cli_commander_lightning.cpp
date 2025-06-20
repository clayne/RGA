//=============================================================================
/// Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for CLI Commander interface for compiling with the Lightning Compiler (LC).
//=============================================================================
// C++.
#include <vector>
#include <map>
#include <utility>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <regex>

// Infra.
#include "external/amdt_os_wrappers/Include/osFilePath.h"
#include "external/amdt_os_wrappers/Include/osDirectory.h"
#include "external/amdt_os_wrappers/Include/osApplication.h"

// Shared.
#include "common/rga_entry_type.h"
#include "common/rg_log.h"
#include "common/rga_shared_utils.h"

// Local.
#include "radeon_gpu_analyzer_cli/kc_cli_string_constants.h"
#include "radeon_gpu_analyzer_cli/kc_utils.h"
#include "radeon_gpu_analyzer_cli/kc_xml_writer.h"
#include "radeon_gpu_analyzer_cli/kc_utils_lightning.h"
#include "radeon_gpu_analyzer_cli/kc_cli_commander_lightning.h"
#include "radeon_gpu_analyzer_cli/kc_statistics_device_props.h"

// Backend.
#include "radeon_gpu_analyzer_backend/be_program_builder_lightning.h"
#include "radeon_gpu_analyzer_backend/be_string_constants.h"
#include "radeon_gpu_analyzer_backend/be_utils.h"

// *****************************************
// *** INTERNALLY LINKED SYMBOLS - START ***
// *****************************************

// Targets of Lightning Compiler in LLVM format and corresponding DeviceInfo names.
static const std::vector<std::pair<std::string, std::string>> kLcLlvmTargetsToDeviceInfoTargets = {
    {"gfx801", "carrizo"},  
    {"gfx802", "tonga"},    
    {"gfx803", "fiji"},     
    {"gfx803", "ellesmere"}, 
    {"gfx803", "baffin"},   
    {"gfx803", "gfx804"},
    {"gfx900", "gfx900"},   
    {"gfx902", "gfx902"},   
    {"gfx904", "gfx904"},   
    {"gfx906", "gfx906"},    
    {"gfx908", "gfx908"},   
    {"gfx90a", "gfx90a"},
    {"gfx90c", "gfx90c"},   
    {"gfx942", "gfx942"},   
    {"gfx1010", "gfx1010"}, 
    {"gfx1011", "gfx1011"},  
    {"gfx1012", "gfx1012"}, 
    {"gfx1030", "gfx1030"},
    {"gfx1031", "gfx1031"}, 
    {"gfx1032", "gfx1032"}, 
    {"gfx1034", "gfx1034"}, 
    {"gfx1035", "gfx1035"},  
    {"gfx1100", "gfx1100"}, 
    {"gfx1101", "gfx1101"},
    {"gfx1102", "gfx1102"}, 
    {"gfx1103", "gfx1103"}, 
    {"gfx1150", "gfx1150"}, 
    {"gfx1151", "gfx1151"},  
    {"gfx1152", "gfx1152"},
    {"gfx1200", "gfx1200"}, 
    {"gfx1201", "gfx1201"}};

// For some devices, clang does not accept device names that RGA gets from DeviceInfo.
// This table maps the DeviceInfo names to names accepted by clang for such devices.
static const std::map<std::string, std::string>
kLcDeviceInfoToClangDeviceMap = { {"ellesmere", "polaris10"},
                                  {"baffin",    "polaris11"},
                                  {"gfx804",    "gfx803"} };

// Default target for Lightning Compiler (the latest supported target).
static const std::string  kLcDefaultTarget = kLcLlvmTargetsToDeviceInfoTargets.rbegin()->second;

static const gtString  kTempBinaryFilename                  = L"rga_lc_ocl_out";
static const gtString  kTempBinaryFileExtension             = L"bin";
static const gtString  kTempIsaFilename                     = L"rga_lc_isa_";
static const gtString  kTempIsaFileExtension                = L"isa";

static const std::string  kCompilerVersionToken             = "clang version ";
static const std::string  kCompilerWarningToken             = "warning:";

static const std::string  kLcIsaInstructionSuffix1          = "_e32";
static const std::string  kLcIsaInstructionSuffix2          = "_e64";

static const std::string  kLcIsaBranchToken                 = "branch";
static const std::string  kIsaCallToken                     = "call";

static const std::string  kIsaInstructionAddressStartToken  = "//";
static const std::string  kIsaInstructionAddressEndToken    = ":";
static const std::string  kIsaCommentStartToken             = ";";

static const std::string  kStrDx11NaValue                       = "N/A";

// Error messages.
static const char* kStrErrorOpenclOfflineCannotFindKernel = "Error: cannot find OpenCL kernel: ";
static const char* kStrErrorOpenclOfflineUnknownDevice1 = "Error: unknown device name provided: ";
static const char* kStrErrorOpenclOfflineUnknownDevice2 = ". Cannot compile for this target.";
static const char* kStrErrorOpenclOfflineFailedToCreateTempFile = "Error: failed to create a temp file.";
static const char* kStrErrorOpenclOfflineLlvmIrDisassemblyFailure = "Error: failed to generate LLVM IR disassembly.";

// Warning messages.
static const char* kStrWarningOpenclOfflineUsingExtraDevice1 = "Warning: using unknown target GPU: ";
static const char* kStrWarningRocmclUsingExtraDevice2 = "; successful compilation and analysis are not guaranteed.";

// Info messages.
static const char* kStrInfoOpenclOfflinePerformingLiveregAnalysis = "Performing live register analysis";
static const char* kStrInfoOpenclOfflineExtractingCfg = "Extracting control flow graph";


static const size_t  kIsaInstruction64BitCodeTextSize   = 16;
static const int     kIsaInstruction64BitBytes          = 8;
static const int     kIsaInstruction32BitBytes          = 4;

// ***************************************
// *** INTERNALLY LINKED SYMBOLS - END ***
// ***************************************

// Returns the list of additional LC targets specified in the "additional-targets" file.
static std::vector<std::string> GetExtraTargetList()
{
    static const wchar_t* kLcExtraTargetsFilename = L"additional-targets";
    std::vector<std::string> device_list;
    osFilePath targets_file_path;
    osGetCurrentApplicationPath(targets_file_path, false);
    targets_file_path.appendSubDirectory(kLcOpenclRootDir);
    targets_file_path.setFileName(kLcExtraTargetsFilename);
    targets_file_path.clearFileExtension();

    std::ifstream targets_file(targets_file_path.asString().asASCIICharArray());
    if (targets_file.good())
    {
        std::string device;
        while (std::getline(targets_file, device))
        {
            if (device.find("//") == std::string::npos)
            {
                // Save the target name in lower case to avoid case-sensitivity issues.
                std::transform(device.begin(), device.end(), device.begin(), [](const char& c) {return static_cast<char>(std::tolower(c)); });
                device_list.push_back(device);
            }
        }
    }

    return device_list;
}

static void  LogPreStep(const std::string& msg, const std::string& device = "")
{
    std::cout << msg << device << "... ";
}

static void  LogResult(bool result)
{
    std::cout << (result ? kStrInfoSuccess : kStrInfoFailed) << std::endl;
}

beKA::beStatus KcCLICommanderLightning::Init(const Config& config, LoggingCallbackFunction log_callback)
{
    log_callback_ = log_callback;
    compiler_paths_  = {config.compiler_bin_path, config.compiler_inc_path, config.compiler_lib_path};
    should_print_cmd_   = config.print_process_cmd_line;
    return beKA::kBeStatusSuccess;
}

bool KcCLICommanderLightning::InitRequestedAsicListLC(const Config& config)
{
    bool ret = false;

    if (config.asics.empty())
    {
        // Use default target if no target is specified by user.
        targets_.insert(kLcDefaultTarget);
        ret = true;
    }
    else
    {
        for (std::string device : config.asics)
        {
            std::set<std::string> supported_targets;
            std::string matched_arch_name;

            [[maybe_unused]] bool is_supported_target_extracted = GetSupportedTargets(supported_targets);
            assert(is_supported_target_extracted);

            // If the device is specified in the LLVM format, convert it to the DeviceInfo format.
            auto llvm_device = std::find_if(kLcLlvmTargetsToDeviceInfoTargets.cbegin(), kLcLlvmTargetsToDeviceInfoTargets.cend(),
                                           [&](const std::pair<std::string, std::string>& d){ return (d.first == device);});
            if (llvm_device != kLcLlvmTargetsToDeviceInfoTargets.cend())
            {
                device = llvm_device->second;
            }

            // Try to detect device.
            if ((KcUtils::FindGPUArchName(device, matched_arch_name, true, true)) == true)
            {
                // Check if the matched architecture name is present in the list of supported devices.
                for (std::string supported_device : supported_targets)
                {
                    if (RgaSharedUtils::ToLower(matched_arch_name).find(supported_device) != std::string::npos)
                    {
                        targets_.insert(supported_device);
                        ret = true;
                        break;
                    }
                }
            }

            if (!ret)
            {
                // Try additional devices from "additional-targets" file.
                std::vector<std::string> extra_devices = GetExtraTargetList();
                std::transform(device.begin(), device.end(), device.begin(), [](const char& c) {return static_cast<char>(std::tolower(c)); });
                if (std::find(extra_devices.cbegin(), extra_devices.cend(), device) != extra_devices.cend())
                {
                    RgLog::stdErr << kStrWarningOpenclOfflineUsingExtraDevice1 << device << kStrWarningRocmclUsingExtraDevice2 << std::endl << std::endl;
                    targets_.insert(device);
                    ret = true;
                }
            }

            if (!ret)
            {
                RgLog::stdErr << kStrErrorOpenclOfflineUnknownDevice1 << device << kStrErrorOpenclOfflineUnknownDevice2 << std::endl << std::endl;
            }
        }
    }

    return !targets_.empty();
}

bool KcCLICommanderLightning::Compile(const Config& config)
{
    bool ret = false;

    if (InitRequestedAsicListLC(config))
    {
        beKA::beStatus result = beKA::kBeStatusSuccess;

        // Prepare OpenCL options and defines.
        OpenCLOptions options;
        options.selected_devices = targets_;
        options.defines = config.defines;
        options.include_paths = config.include_path;
        options.opencl_compile_options = config.opencl_options;
        options.optimization_level = config.opt_level;
        options.line_numbers = config.is_line_numbers_required;
        options.should_dump_il = !config.il_file.empty();

        // Run the back-end compilation procedure.
        switch (config.mode)
        {
        case beKA::kModeOpenclOffline:
            result = CompileOpenCL(config, options);
            break;
        default:
            result = beKA::kBeStatusGeneralFailed;
            break;
        }

        // Generate CSV files with parsed ISA if required.
        if (config.is_parsed_isa_required && (result == beKA::kBeStatusSuccess || targets_.size() > 1))
        {
            KcUtilsLightning util(output_metadata_, should_print_cmd_, log_callback_);
            result = util.ParseIsaFilesToCSV(config.is_line_numbers_required) ? beKA::beStatus::kBeStatusSuccess : beKA::beStatus::kBeStatusParseIsaToCsvFailed;
        }

        ret = (result == beKA::kBeStatusSuccess);
    }

    return ret;
}

void KcCLICommanderLightning::Version(Config& config, LoggingCallbackFunction callback)
{
    bool  ret;
    std::stringstream log;
    KcCliCommander::Version(config, callback);

    std::string output_text = "", version = "";
    beKA::beStatus status = BeProgramBuilderLightning::GetCompilerVersion(beKA::RgaMode::kModeOpenclOffline, config.compiler_bin_path,
                                                                           config.print_process_cmd_line, output_text);
    ret = (status == beKA::kBeStatusSuccess);
    if (ret)
    {
        size_t  offset = output_text.find(kCompilerVersionToken);
        if (offset != std::string::npos)
        {
            offset += kCompilerVersionToken.size();
            size_t  offset1 = output_text.find(" ", offset);
            size_t  offset2 = output_text.find("\n", offset);
            if (offset1 != std::string::npos && offset2 != std::string::npos)
            {
                size_t end_offset = std::min<size_t>(offset1, offset2);
                version = output_text.substr(0, end_offset);
            }
        }
    }

    if (ret)
    {
        const char* kStrOpenclOfflineCompilerVersionPrefix = "OpenCL Compiler: AMD Lightning Compiler - ";
        log << kStrOpenclOfflineCompilerVersionPrefix << version << std::endl;
    }

    LogCallback(log.str());
}

bool KcCLICommanderLightning::GenerateOpenclOfflineVersionInfo(const std::string& filename)
{
    std::set<std::string> targets;

    // Get the list of supported GPUs for current mode.
    bool result = GetSupportedTargets(targets);

    // Add the list of supported GPUs to the Version Info file.
    result = result && KcXmlWriter::AddVersionInfoGPUList(beKA::RgaMode::kModeOpenclOffline, targets, filename);

    return result;
}

bool KcCLICommanderLightning::PrintAsicList(const Config&)
{
    std::set<std::string> targets;
    bool ret = GetSupportedTargets(targets);
    ret = ret && KcUtils::PrintAsicList(targets);

    if (ret)
    {
        // Print additional OpenCL Lightning Compiler target from the "additional-targets" file.
        std::vector<std::string> extra_targets = GetExtraTargetList();
        if (!extra_targets.empty())
        {
            static const char* kStrOpenclOfflineExtraDeviceListTitle = "Additional GPU targets (Warning: correct compilation and analysis are not guaranteed):";
            static const char* kStrOpenclOfflineDeviceListOffset = "    ";
            RgLog::stdOut << std::endl << kStrOpenclOfflineExtraDeviceListTitle << std::endl << std::endl;
            for (const std::string& device : extra_targets)
            {
                RgLog::stdOut << kStrOpenclOfflineDeviceListOffset << device << std::endl;
            }
            RgLog::stdOut << std::endl;
        }
    }
    return ret;
}

// Print warnings reported by compiler to stderr.
static bool DumpCompilerWarnings(const std::string& compiler_std_err)
{
    bool found_warnings = compiler_std_err.find(kCompilerWarningToken) != std::string::npos;
    if (found_warnings)
    {
        RgLog::stdErr << std::endl << compiler_std_err << std::endl;
    }
    return found_warnings;
}

beKA::beStatus KcCLICommanderLightning::CompileOpenCL(const Config& config, const OpenCLOptions& ocl_options)
{
    beKA::beStatus status = beKA::beStatus::kBeStatusSuccess;

    // Run LC compiler for all requested devices
    for (const std::string& device : ocl_options.selected_devices)
    {
        std::string  error_text;
        LogPreStep(kStrInfoCompiling, device);
        std::string  bin_filename;

        // Adjust the device name if necessary.
        std::string clang_device = device;
        if (kLcDeviceInfoToClangDeviceMap.count(clang_device) > 0)
        {
            clang_device = kLcDeviceInfoToClangDeviceMap.at(device);
        }

        // Update the binary and ISA names for current device.
        beKA::beStatus current_status = AdjustBinaryFileName(config, device, bin_filename);

        // If file with the same name exist, delete it.
        KcUtils::DeleteFile(bin_filename);

        // Prepare a list of source files.
        std::vector<std::string>  src_filenames;
        for (const std::string& input_file : config.input_files)
        {
            src_filenames.push_back(input_file);
        }

        if (current_status != beKA::beStatus::kBeStatusSuccess)
        {
            KcUtilsLightning::LogErrorStatus(current_status, error_text);
            continue;
        }

        // Compile source to binary.
        current_status = BeProgramBuilderLightning::CompileOpenCLToBinary(compiler_paths_,
                                                                         ocl_options,
                                                                         src_filenames,
                                                                         bin_filename,
                                                                         clang_device,
                                                                         should_print_cmd_,
                                                                         error_text);
        LogResult(current_status == beKA::beStatus::kBeStatusSuccess);

        if (current_status == beKA::beStatus::kBeStatusSuccess)
        {
            // If "dump IL" option is passed to the Lightning Compiler, it should dump the IL to stderr.
            if (ocl_options.should_dump_il)
            {
                current_status = DumpIL(config, ocl_options, src_filenames, device, clang_device, error_text);
            }
            else if (config.is_warnings_required)
            {
                // Pass the warnings printed by the compiler to RGA stderr.
                DumpCompilerWarnings(error_text);
            }

            // Disassemble binary to ISA text.
            if (!config.isa_file.empty() ||
                !config.analysis_file.empty() ||
                !config.livereg_analysis_file.empty() || 
                !config.sgpr_livereg_analysis_file.empty() || 
                !config.block_cfg_file.empty() ||
                !config.inst_cfg_file.empty())
            {
                LogPreStep(kStrInfoExtractingIsaForDevice, device);
                current_status = DisassembleBinary(bin_filename, config.isa_file, clang_device, device, config.function, config.is_line_numbers_required, error_text);
                LogResult(current_status == beKA::beStatus::kBeStatusSuccess);

                assert(current_status == beKA::beStatus::kBeStatusSuccess);
                // Propagate the binary file name to the Output Files Metadata table.
                if (current_status == beKA::beStatus::kBeStatusSuccess)
                {
                    for (auto& output_md_node : output_metadata_)
                    {
                        const std::string& md_device = output_md_node.first.first;
                        if (md_device == device)
                        {
                            output_md_node.second.bin_file = bin_filename;
                            output_md_node.second.is_bin_file_temp = config.binary_output_file.empty();
                        }
                    }
                }
            }
            else
            {
                output_metadata_[{device, ""}] = RgOutputFiles(RgaEntryType::kOpenclKernel, "", bin_filename);
            }
        }
        else
        {
            // Store error status to the metadata.
            RgOutputFiles output(RgaEntryType::kOpenclKernel, "", "");
            output.status = false;
            output_metadata_[{device, ""}] = output;
        }

        status = (current_status == beKA::beStatus::kBeStatusSuccess ? status : current_status);
        KcUtilsLightning::LogErrorStatus(current_status, error_text);
    }

    return status;
}

beKA::beStatus KcCLICommanderLightning::DisassembleBinary(const std::string& binFileName,
                                                          const std::string& userIsaFileName,
                                                          const std::string& clangDevice,
                                                          const std::string& rgaDevice,
                                                          const std::string& kernel,
                                                          bool lineNumbers,
                                                          std::string& error_text)
{
    std::string  out_isa_text;
    std::vector<std::string>  kernel_names;
    beKA::beStatus status = BeProgramBuilderLightning::DisassembleBinary(compiler_paths_.bin, binFileName,
        clangDevice, lineNumbers, should_print_cmd_, out_isa_text, error_text);

    if (status == beKA::kBeStatusSuccess)
    {
        status = BeProgramBuilderLightning::ExtractKernelNames(compiler_paths_.bin, binFileName,
            should_print_cmd_, kernel_names);
    }
    else
    {
        // Store error status to the metadata.
        RgOutputFiles output(RgaEntryType::kOpenclKernel, "", "");
        output.status = false;
        output_metadata_[{rgaDevice, ""}] = output;
    }

    if (status == beKA::kBeStatusSuccess)
    {
        if (!kernel.empty() && std::find(kernel_names.cbegin(), kernel_names.cend(), kernel) == kernel_names.cend())
        {
            error_text = std::string(kStrErrorOpenclOfflineCannotFindKernel) + kernel;
            status = beKA::kBeStatusWrongKernelName;
        }
        else
        {
            status = SplitISA(binFileName, out_isa_text, userIsaFileName, rgaDevice, kernel, kernel_names) ?
                         beKA::kBeStatusSuccess : beKA::kBeStatusLightningSplitIsaFailed;
        }
    }

    return status;
}

void KcCLICommanderLightning::RunCompileCommands(const Config& config, LoggingCallbackFunction)
{
    bool is_multiple_devices = (config.asics.size() > 1);
    bool status = Compile(config);
    KcUtilsLightning util(output_metadata_, should_print_cmd_, log_callback_);
    
    // Extract Statistics if required.
    if (status || is_multiple_devices)
    {
        util.ExtractStatistics(config);
    }

    // Block post-processing until quality of analysis engine improves when processing llvm disassembly.
    bool is_livereg_required = !config.livereg_analysis_file.empty();
    if (is_livereg_required)
    {
        // Perform Live Registers analysis if required.
        if (status || is_multiple_devices)
        {
            util.PerformLiveVgprAnalysis(config);
        }
    }

    bool is_sgpr_livereg_required = !config.sgpr_livereg_analysis_file.empty();
    if (is_sgpr_livereg_required)
    {
        // Perform Live Registers analysis if required.
        if (status || is_multiple_devices)
        {
            util.PerformLiveSgprAnalysis(config);
        }
    }

    bool is_cfg_required = (!config.block_cfg_file.empty() || !config.inst_cfg_file.empty());
    if (is_cfg_required)
    {
        // Extract Control Flow Graph.
        if (status || is_multiple_devices)
        {
            util.ExtractCFG(config);
        }
    }

    // Extract CodeObj metadata if required.
    if ((status || is_multiple_devices) && !config.metadata_file.empty())
    {
        util.ExtractMetadata(compiler_paths_, config.metadata_file);
    }
}

beKA::beStatus KcCLICommanderLightning::AdjustBinaryFileName(const Config&      config,
                                                             const std::string& device,
                                                             std::string&       bin_filename)
{
    beKA::beStatus  status = beKA::kBeStatusSuccess;

    gtString name = L"";
    std::string user_bin_name = config.binary_output_file;

    // If binary output file name is not provided, create a binary file in the temp folder.
    if (user_bin_name == "")
    {
        user_bin_name = KcUtils::ConstructTempFileName(kTempBinaryFilename, kTempBinaryFileExtension).asASCIICharArray();
        if (user_bin_name == "")
        {
            status = beKA::kBeStatusGeneralFailed;
        }
    }

    if (status == beKA::kBeStatusSuccess)
    {
        name = L"";
        KcUtils::ConstructOutputFileName(user_bin_name, "", kStrDefaultExtensionBin, "", device, name);
        bin_filename = name.asASCIICharArray();
    }

    return status;
}

static void  GatherBranchTargets(std::stringstream& isa, std::unordered_map<std::string, bool>& branch_targets)
{
    // The format of branch instruction text:
    //
    //     s_cbranch_scc1 BB0_3        // 000000001110: BF85001C
    //           ^         ^                    ^          ^
    //           |         |                    |          |
    //      instruction  label               offset       code

    std::string  isa_line;

    // Skip lines before the actual ISA code.
    while (std::getline(isa, isa_line) && isa_line.find(kLcKernelIsaHeader3) == std::string::npos) {}

    // Gather target labels of all branch instructions.
    while (std::getline(isa, isa_line))
    {
        size_t  inst_end_offset, branch_token_offset, instOffset = isa_line.find_first_not_of(" \t");
        if (instOffset != std::string::npos)
        {
            if ((branch_token_offset = isa_line.find(kLcIsaBranchToken, instOffset)) != std::string::npos ||
                (branch_token_offset = isa_line.find(kIsaCallToken, instOffset)) != std::string::npos)
            {
                if ((inst_end_offset = isa_line.find_first_of(" \t", instOffset)) != std::string::npos &&
                    branch_token_offset < inst_end_offset)
                {
                    // Found branch instruction. Add its target label to the list.
                    size_t  label_start_offset, label_end_offset;
                    if ((label_start_offset = isa_line.find_first_not_of(" \t", inst_end_offset)) != std::string::npos &&
                        isa_line.compare(label_start_offset, kIsaInstructionAddressStartToken.size(), kIsaInstructionAddressStartToken) != 0 &&
                        ((label_end_offset = isa_line.find_first_of(" \t", label_start_offset)) != std::string::npos))
                    {
                        branch_targets[isa_line.substr(label_start_offset, label_end_offset - label_start_offset)] = true;
                    }
                }
            }
        }
    }
    isa.clear();
    isa.seekg(0);
}

// Checks if "isa_line" is a label that is not in the list of branch targets.
bool  IsUnreferencedLabel(const std::string& isa_line, const std::unordered_map<std::string, bool>& branch_targets)
{
    bool  ret = false;

    // Looking for strings of the pattern 'anylabel:' that are not function labels.
    size_t colon_indx = isa_line.find(':');
    size_t line_size = isa_line.size();
    if ((colon_indx == (line_size - 1)) && (line_size > 1))
    {
        std::string branch_name = isa_line.substr(0, isa_line.size() - 1);
        ret = (branch_targets.find(branch_name) == branch_targets.end());
    }

    return ret;
}

// Remove non-standard instruction suffixes.
static void  FilterISALine(std::string & isa_line)
{
    size_t  offset = isa_line.find_first_not_of(" \t");
    if (offset != std::string::npos)
    {
        offset = isa_line.find_first_of(" ");
    }
    if (offset != std::string::npos)
    {
        size_t  suffix_length = 0;
        if (offset >= kLcIsaInstructionSuffix1.size() &&
            isa_line.substr(offset - kLcIsaInstructionSuffix1.size(), kLcIsaInstructionSuffix1.size()) == kLcIsaInstructionSuffix1)
        {
            suffix_length = kLcIsaInstructionSuffix1.size();
        }
        else if (offset >= kLcIsaInstructionSuffix2.size() &&
            isa_line.substr(offset - kLcIsaInstructionSuffix2.size(), kLcIsaInstructionSuffix2.size()) == kLcIsaInstructionSuffix2)
        {
            suffix_length = kLcIsaInstructionSuffix2.size();
        }
        // Remove the suffix.
        if (suffix_length != 0)
        {
            isa_line.erase(offset - suffix_length, suffix_length);
            // Restore the alignment of byte encoding.
            if ((offset = isa_line.find("//", offset)) != std::string::npos)
            {
                isa_line.insert(offset, suffix_length, ' ');
            }
        }
    }
}

// The Lightning Compiler may append useless code for some library functions to the ISA disassembly.
// This function eliminates such code.
// It also also removes unreferenced labels and non-standard instruction suffixes.
bool  KcCLICommanderLightning::ReduceISA(const std::string& binFile, IsaMap& kernel_isa_text)
{
    bool  ret = false;
    for (auto& kernel_isa : kernel_isa_text)
    {
        int  code_size = BeProgramBuilderLightning::GetKernelCodeSize(compiler_paths_.bin, binFile, kernel_isa.first, should_print_cmd_);
        assert(code_size != -1);
        if (code_size != -1)
        {
            // Copy ISA lines to new stream. Stop when found an instruction with address > codeSize.
            std::stringstream  old_isa, new_isa, address_stream;
            old_isa.str(kernel_isa.second);
            std::string  isa_line;
            int  address, address_offset = -1;

            // Gather the target labels of all branch instructions.
            std::unordered_map<std::string, bool>  branch_targets;
            branch_targets.clear();
            GatherBranchTargets(old_isa, branch_targets);

            // Skip lines before the actual ISA code.
            while (std::getline(old_isa, isa_line) && new_isa << isa_line << std::endl &&
                isa_line.find(kLcKernelIsaHeader3) == std::string::npos) {}

            while (std::getline(old_isa, isa_line))
            {
                // Add the ISA line to the new ISA text if it's not an unreferenced label.
                if (!IsUnreferencedLabel(isa_line, branch_targets))
                {
                    if (isa_line.find(" <") != 0)
                    {
                        size_t branch_label_start = isa_line.find(" <") + 2;
                        size_t branch_label_end = isa_line.find(">:");
                        size_t address_end = isa_line.find_first_of(" ");
                        if (branch_label_end != std::string::npos)
                        {
                            // If this is a branch label, reformat and add the string so the RGA GUI recognizes the syntax.
                            std::string new_branch_label = isa_line.substr(0, address_end + 1) + isa_line.substr(branch_label_start, branch_label_end - (branch_label_start)) + ":";
                            new_isa << new_branch_label << std::endl;
                        }
                        else
                        {
                            // Add the line as is.
                            new_isa << isa_line << std::endl;
                        }
                    }
                }

                // Check if this instruction is the last one and we have to stop here.
                // Skip comment lines generated by disassembler.
                if (isa_line.find(kIsaCommentStartToken, 0) != 0)
                {

                    // Format of ISA disassembly instruction (64-bit and 32-bit):
                    //  s_load_dwordx2 s[0:1], s[6:7], 0x0     // 000000001108: C0060003 00000000
                    //  v_add_u32 v0, s8, v0                   // 000000001134: 68000008
                    //                                            `-- addr --'
                    size_t  address_start, address_end;

                    FilterISALine(isa_line);

                    if ((address_start = isa_line.find(kIsaInstructionAddressStartToken)) != std::string::npos &&
                        (address_end = isa_line.find(kIsaInstructionAddressEndToken, address_start)) != std::string::npos)
                    {
                        address_start += (kIsaInstructionAddressStartToken.size());
                        address_stream.str(isa_line.substr(address_start, address_end - address_start));
                        address_stream.clear();
                        int inst_size = (isa_line.size() - address_end < kIsaInstruction64BitCodeTextSize) ? kIsaInstruction32BitBytes : kIsaInstruction64BitBytes;
                        if (address_stream >> std::hex >> address)
                        {
                            // address_offset is the binary address of 1st instruction.
                            address_offset = (address_offset == -1 ? address : address_offset);
                            if ((address - address_offset + inst_size) >= code_size)
                            {
                                ret = true;
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }

            if (ret)
            {
                kernel_isa.second = new_isa.str();
            }
        }
    }

    return ret;
}

bool KcCLICommanderLightning::SplitISA(const std::string& bin_file, const std::string& isa_text,
                                       const std::string& user_isa_file_name, const std::string& device,
                                       const std::string& kernel, const std::vector<std::string>& kernel_names)
{
    // kernelIsaTextMap maps kernel name --> kernel ISA text.
    IsaMap kernel_isa_text_map;
    bool ret, is_isa_file_temp = user_isa_file_name.empty();

    // Replace labels of format "address   <label_name>:" with
    //  "label_name:"
    std::string label_pattern_prefix("[0-9a-zA-Z]+ <");
    std::regex label_prefix_regex(label_pattern_prefix);
    std::regex label_suffix_regex(">:");
    std::string isa_text_clean_prefix = std::regex_replace(isa_text, label_prefix_regex, "");
    std::string new_isa_text = std::regex_replace(isa_text_clean_prefix, label_suffix_regex, ":");

    // Split ISA text into per-kernel fragments.
    ret = SplitISAText(new_isa_text, kernel_names, kernel_isa_text_map);

    // Eliminate the useless code.
    ret = ret && ReduceISA(bin_file, kernel_isa_text_map);

    // Store per-kernel ISA texts to separate files and launch livereg tool for each file.
    if (ret)
    {
        // isaTextMapItem is a pair{kernelName, kernelIsaText}.
        for (const auto &isa_text_map_item : kernel_isa_text_map)
        {
            // Skip the kernels that are not requested.
            if (!kernel.empty() && kernel != isa_text_map_item.first)
            {
                continue;
            }

            gtString  isa_filename;
            std::string function_name = isa_text_map_item.first;

            if (is_isa_file_temp)
            {
                gtString  base_isa_filename(kTempIsaFilename);
                base_isa_filename << device.c_str() << "_" << function_name.c_str();
                isa_filename = KcUtils::ConstructTempFileName(base_isa_filename, kTempIsaFileExtension);
            }
            else
            {
                KcUtils::ConstructOutputFileName(user_isa_file_name, "", kStrDefaultExtensionIsa,
                                                 function_name, device, isa_filename);
            }
            if (!isa_filename.isEmpty())
            {
                if (KcUtils::WriteTextFile(isa_filename.asASCIICharArray(), isa_text_map_item.second, log_callback_))
                {
                    RgOutputFiles  outFiles = RgOutputFiles(RgaEntryType::kOpenclKernel, isa_filename.asASCIICharArray());
                    outFiles.is_isa_file_temp = is_isa_file_temp;
                    output_metadata_[{device, isa_text_map_item.first}] = outFiles;
                }
            }
            else
            {
                std::stringstream  error_msg;
                error_msg << kStrErrorOpenclOfflineFailedToCreateTempFile << std::endl;
                log_callback_(error_msg.str());
                ret = false;
            }
        }
    }

    return ret;
}

bool KcCLICommanderLightning::SplitISAText(const std::string& isa_text,
                                           const std::vector<std::string>& kernel_names,
                                           IsaMap& kernel_isa_map) const
{
    bool  status = true;
    const std::string  LABEL_NAME_END_TOKEN = ":\n";
    const std::string  BLOCK_END_TOKEN = "\n\n";
    size_t label_name_start = 0, label_name_end = 0, kernel_isa_end = 0;

    label_name_start = isa_text.find_first_not_of('\n');

    std::vector<std::pair<size_t, size_t>> kernel_start_offsets;
    if (!isa_text.empty())
    {
        while ((label_name_end = isa_text.find(LABEL_NAME_END_TOKEN, label_name_start)) != std::string::npos)
        {
            // Check if this contains a kernel name.
            std::string  label_name = isa_text.substr(label_name_start, label_name_end - label_name_start);
            if (std::count(kernel_names.begin(), kernel_names.end(), label_name) != 0)
            {
                kernel_start_offsets.push_back({ label_name_start, label_name_end - label_name_start });
            }
            if ((label_name_start = isa_text.find(BLOCK_END_TOKEN, label_name_end)) == std::string::npos)
            {
                // End of file.
                break;
            }
            else
            {
                label_name_start += BLOCK_END_TOKEN.size();
            }
        }
    }

    // Split the ISA text using collected offsets of kernel names.
    for (size_t i = 0, size = kernel_start_offsets.size(); i < size; i++)
    {
        size_t  isa_text_start = kernel_start_offsets[i].first;
        size_t  isa_text_end = (i < size - 1 ? kernel_start_offsets[i + 1].first - 1 : isa_text.size());
        if (isa_text_start <= isa_text_end)
        {
            const std::string& kernel_isa  = isa_text.substr(isa_text_start, isa_text_end - isa_text_start);
            const std::string& kernel_name = isa_text.substr(kernel_start_offsets[i].first, kernel_start_offsets[i].second);
            kernel_isa_map[kernel_name]    = KcUtilsLightning::PrefixWithISAHeader(kernel_name, kernel_isa);
            label_name_start = kernel_isa_end + BLOCK_END_TOKEN.size();
        }
        else
        {
            status = false;
            break;
        }
    }

    return status;
}

bool KcCLICommanderLightning::ListEntries(const Config& config, LoggingCallbackFunction callback)
{
    return ListEntriesOpenclOffline(config, callback);
}

bool KcCLICommanderLightning::ListEntriesOpenclOffline(const Config& config, LoggingCallbackFunction callback)
{
    bool ret = true;
    std::string filename;
    RgEntryData entry_data;
    std::stringstream msg;

    if (config.mode != beKA::RgaMode::kModeOpenclOffline)
    {
        msg << kStrErrorCommandNotSupported << std::endl;
        ret = false;
    }
    else
    {
        if (config.input_files.size() == 1)
        {
            filename = config.input_files[0];
        }
        else if (config.input_files.size() > 1)
        {
            msg << kStrErrorSingleInputFileExpected << std::endl;
            ret = false;
        }
        else
        {
            msg << kStrErrorNoInputFile << std::endl;
            ret = false;
        }
    }

    if (ret && (ret = KcUtilsLightning::ExtractEntries(filename, config, compiler_paths_, entry_data)) == true)
    {
        // Sort the entry names in alphabetical order.
        std::sort(entry_data.begin(), entry_data.end(),
            [](const std::tuple<std::string, int, int>& a, const std::tuple<std::string, int, int>& b) {return (std::get<0>(a) < std::get<0>(b)); });

        // Dump the entry points.
        for (const auto& data_item : entry_data)
        {
            msg << std::get<0>(data_item) << ": " << std::get<1>(data_item) << "-" << std::get<2>(data_item) << std::endl;
        }
        msg << std::endl;
    }

    callback(msg.str());

    return ret;
}

bool KcCLICommanderLightning::GetSupportedTargets(std::set<std::string>& targets, bool)
{
    // Gather the supported devices in DeviceInfo format.
    targets.clear();

    for (const auto& d : kLcLlvmTargetsToDeviceInfoTargets)
    {
        targets.insert(d.second);
    }

    return !targets.empty();
}

bool KcCLICommanderLightning::RunPostCompileSteps(const Config& config)
{
    bool ret = false;

    if (!config.session_metadata_file.empty())
    {
        ret = GenerateSessionMetadata(config, compiler_paths_);
        if (!ret)
        {
            std::stringstream msg;
            msg << kStrErrorFailedToGenerateSessionMetdata << std::endl;
            log_callback_(msg.str());
        }
    }

    KcUtilsLightning::DeleteTempFiles(output_metadata_);

    return ret;
}

beKA::beStatus KcCLICommanderLightning::DumpIL(const Config&                   config,
                                         const OpenCLOptions&            user_options,
                                         const std::vector<std::string>& src_file_names,
                                         const std::string&              device,
                                         const std::string&              clang_device,
                                         std::string&                    error_text)
{
    beKA::beStatus status = beKA::beStatus::kBeStatusSuccess;

    // Generate new options instructing to generate LLVM IR disassembly.
    OpenCLOptions ocl_options_llvm_ir                       = user_options;
    ocl_options_llvm_ir.should_generate_llvm_ir_disassembly = true;

    for (const auto& src_file_name_with_ext : src_file_names)
    {
        // Convert the src kernel input file name to gtString.
        gtString src_file_name_with_ext_as_gtstr;
        src_file_name_with_ext_as_gtstr << src_file_name_with_ext.c_str();
        osFilePath src_file_path(src_file_name_with_ext_as_gtstr);

        // Extract the kernel's file name without directory and extension.
        gtString src_file_name;
        assert(!src_file_path.isDirectory());
        src_file_path.getFileName(src_file_name);

        gtString il_filename;
        KcUtils::ConstructOutputFileName(config.il_file, "", kStrDefaultExtensionLlvmir, src_file_name.asASCIICharArray(), device, il_filename);

        // Invoking clang with "-emit-llvm -S" for multiple .cl files is not supported.
        // Clang should be invoked with one input file at a time.
        status = BeProgramBuilderLightning::CompileOpenCLToLlvmIr(compiler_paths_,
                                                                  ocl_options_llvm_ir,
                                                                  std::vector<std::string>{src_file_name_with_ext},
                                                                  il_filename.asASCIICharArray(),
                                                                  clang_device,
                                                                  should_print_cmd_,
                                                                  error_text);
        if (status != beKA::beStatus::kBeStatusSuccess)
        {
            std::stringstream msg;
            msg << kStrErrorOpenclOfflineLlvmIrDisassemblyFailure << std::endl;
            error_text.append(msg.str());
        }
    }
    return status;
}

bool KcCLICommanderLightning::GenerateSessionMetadata(const Config& config, const CmpilerPaths& compiler_paths) const
{
    RgFileEntryData file_kernel_data;
    bool            ret = !config.session_metadata_file.empty();
    assert(ret);

    if (ret)
    {
        for (const std::string& input_file : config.input_files)
        {
            RgEntryData entry_data;
            ret = ret && KcUtilsLightning::ExtractEntries(input_file, config, compiler_paths, entry_data);
            if (ret)
            {
                file_kernel_data[input_file] = entry_data;
            }
        }
    }

    if (ret && !output_metadata_.empty())
    {
        ret = KcXmlWriter::GenerateClSessionMetadataFile(config.session_metadata_file, file_kernel_data, output_metadata_);
    }

    return ret;
}
