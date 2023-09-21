//======================================================================
// Copyright 2023 Advanced Micro Devices, Inc. All rights reserved.
//======================================================================

#pragma once

// C++.
#include <string>
#include <map>

static const std::string kStrNaValue = "N/A";

// The device properties necessary for generating statistics.
struct DeviceProps
{
    uint64_t available_sgprs;
    uint64_t available_vgprs;
    uint64_t available_lds_bytes;
    uint64_t min_sgprs;
    uint64_t min_vgprs;
};

static const std::map<std::string, DeviceProps> kRgaDeviceProps =
{ 
      {"carrizo",   {102, 256, 65536, 16,  4}},
      {"tonga",     {102, 256, 65536, 16, 64}},
      {"fiji",      {102, 256, 65536, 16,  4}},
      {"ellesmere", {102, 256, 65536, 16,  4}},
      {"baffin",    {102, 256, 65536, 16,  4}},
      {"polaris10", {102, 256, 65536, 16,  4}},
      {"polaris11", {102, 256, 65536, 16,  4}},
      {"gfx804",    {102, 256, 65536, 16,  4}},
      {"gfx900",    {102, 256, 65536, 16,  4}},
      {"gfx902",    {102, 256, 65536, 16,  4}},
      {"gfx904",    {102, 256, 65536, 16,  4}},
      {"gfx906",    {102, 256, 65536, 16,  4}},
      {"gfx908",    {102, 256, 65536, 16,  4}},
      {"gfx90c",    {102, 256, 65536, 16,  4}},
      {"gfx1010",   {106, 256, 65536, 16,  4}},
      {"gfx1011",   {106, 256, 65536, 16,  4}},
      {"gfx1012",   {106, 256, 65536, 16,  4}},
      {"gfx1030",   {106, 256, 65536, 16,  4}},
      {"gfx1031",   {106, 256, 65536, 16,  4}},
      {"gfx1032",   {106, 256, 65536, 16,  4}},   
      {"gfx1034",   {106, 256, 65536, 16,  4}},
      {"gfx1035",   {106, 256, 65536, 16,  4}},
      {"gfx1100",   {106, 256, 65536, 16,  4}},
      {"gfx1101",   {106, 256, 65536, 16,  4}},
      {"gfx1102",   {106, 256, 65536, 16,  4}},
      {"gfx1103",   {106, 256, 65536, 16,  4}} 
};

// Lambda returning "N/A" string if value = -1 or string representation of value itself otherwise.
auto na_or = [](uint64_t val) 
{ 
    return (val == (int64_t)-1 ? kStrNaValue : std::to_string(val)); 
};