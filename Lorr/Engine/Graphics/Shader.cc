#include "Shader.hh"

#include "STL/String.hh"

#include <atlbase.h>
#include <dxc/dxcapi.h>
#include <spirv_hlsl.hpp>
#include <Crypt/CRC.hh>

namespace lr::Graphics
{
struct PragmaOptions
{
    enum PragmaOption : u32
    {
        Stage = 0,
        Count,
    };

    union PragmaOptionsData
    {
        ShaderStage m_stage = ShaderStage::Count;
    };

    PragmaOptionsData &operator[](PragmaOption i) { return m_vars[static_cast<usize>(i)]; }
    const PragmaOptionsData &at(PragmaOption i) const { return m_vars[static_cast<usize>(i)]; }

    PragmaOptionsData m_vars[Count] = {
        [Stage] = ShaderStage::Count,
    };
};

PragmaOptions parse_pragma_options(eastl::string_view shader)
{
    constexpr static eastl::string_view k_pragma_ident = "#pragma ";

    PragmaOptions options = {};

    eastl::string_view line;
    while (ls::get_line(shader, line))
    {
        if (!line.starts_with(k_pragma_ident))
            continue;

        line.remove_prefix(k_pragma_ident.length());
        usize lbracket = line.find('(');
        usize rbracket = line.find(')');
        if (lbracket == -1 && rbracket == -1)
            continue;

        eastl::string_view option = line.substr(0, lbracket);
        eastl::string_view value = line.substr(lbracket + 1, rbracket - option.length() - 1);

        switch (Hash::CRC32DataSV(option))
        {
            case CRC32HashOf("stage"):
            {
                auto get_stage = [value]() -> ShaderStage
                {
                    switch (Hash::CRC32DataSV(value))
                    {
                        case CRC32HashOf("vertex"):
                            return ShaderStage::Vertex;
                        case CRC32HashOf("pixel"):
                            return ShaderStage::Pixel;
                        case CRC32HashOf("compute"):
                            return ShaderStage::Compute;
                        case CRC32HashOf("tess_eval"):
                            return ShaderStage::TessellationEvaluation;
                        case CRC32HashOf("tess_control"):
                            return ShaderStage::TessellationControl;
                        default:
                            return ShaderStage::Count;
                    }
                };
                options[PragmaOptions::Stage].m_stage = get_stage();
                break;
            }
            default:
                LOG_ERROR("Unknown pragma '{}{}'!", k_pragma_ident, line);
        }
    }

    return options;
}

Format to_vk_format(const spirv_cross::SPIRType &type)
{
    using namespace spirv_cross;
    constexpr static Format float_formats[] = { [0] = Format::Float, [1] = Format::Vec2, [2] = Format::Vec3, [3] = Format::Vec4 };
    constexpr static Format int_formats[] = { [0] = Format::Int, [1] = Format::Vec2I, [2] = Format::Vec3I, [3] = Format::Vec4I };
    constexpr static Format uint_formats[] = { [0] = Format::UInt, [1] = Format::Vec2U, [2] = Format::Vec3U, [3] = Format::Vec4U };
    static Format *format_map[] = {
        [SPIRType::Float] = (Format *)float_formats,
        [SPIRType::Int] = (Format *)int_formats,
        [SPIRType::UInt] = (Format *)uint_formats,
    };

    if (type.columns != 1)
        return Format::Unknown;  // Skip matrix types

    return format_map[type.basetype][type.vecsize - 1];
}

eastl::vector<u32> ShaderCompiler::compile_shader(ShaderCompileDesc *desc)
{
    ZoneScoped;

    constexpr static LPCWSTR kHLSLStageMap[] = {
        [(u32)ShaderStage::Vertex] = L"vs_6_7",
        [(u32)ShaderStage::Pixel] = L"ps_6_7",
        [(u32)ShaderStage::Compute] = L"cs_6_7",
        [(u32)ShaderStage::TessellationControl] = L"hs_6_7",
        [(u32)ShaderStage::TessellationEvaluation] = L"ds_6_7",
    };

    DxcBuffer shader_buffer = {
        .Ptr = desc->m_code.data(),
        .Size = desc->m_code.size(),
        .Encoding = 0,
    };

    CComPtr<IDxcCompiler3> compiler = nullptr;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    CComPtr<IDxcUtils> utils = nullptr;
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));

    CComPtr<IDxcIncludeHandler> include_handler = nullptr;
    utils->CreateDefaultIncludeHandler(&include_handler);

    auto options = parse_pragma_options(desc->m_code);
    if (options[PragmaOptions::Stage].m_stage == ShaderStage::Count)
    {
        LOG_ERROR("Shader stage(#pragma stage(...)) is not set");
        return {};
    }
    eastl::vector<LPCWSTR> args = {};
    eastl::vector<eastl::wstring> temp_wstr = {};
    args.push_back(L"-E");
    args.push_back(L"main");
    for (auto &dir : desc->m_include_dirs)
    {
        eastl::string include_str = eastl::format("-I {}", dir);
        args.push_back(temp_wstr.emplace_back(ls::to_wstring(include_str)).data());
    }
    args.push_back(L"-spirv");
    args.push_back(L"-fspv-target-env=vulkan1.3");
    args.push_back(L"-fvk-use-gl-layout");
    args.push_back(L"-no-warnings");
    args.push_back(L"-T");
    args.push_back(kHLSLStageMap[(u32)options[PragmaOptions::Stage].m_stage]);
    for (auto &definition : desc->m_definitions)
    {
        eastl::string def_str = eastl::format("-D{}", definition);
        args.push_back(temp_wstr.emplace_back(ls::to_wstring(def_str)).data());
    }
    if (desc->m_flags & ShaderCompileFlag::GenerateDebugInfo)
        args.push_back(L"-Zi");
    if (desc->m_flags & ShaderCompileFlag::SkipOptimization)
        args.push_back(L"-O0");
    if (desc->m_flags & ShaderCompileFlag::SkipValidation)
        args.push_back(L"-Vd");

    CComPtr<IDxcResult> result = nullptr;
    compiler->Compile(&shader_buffer, args.data(), args.size(), &*include_handler, IID_PPV_ARGS(&result));

    CComPtr<IDxcBlobUtf8> error = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr);
    if (error && error->GetStringLength() > 0)
    {
        LOG_ERROR("Failed to compile shader!\n{}", error->GetStringPointer());
        return {};
    }

    CComPtr<IDxcBlob> output = nullptr;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&output), nullptr);
    assert(output);

    u32 *data = static_cast<u32 *>(output->GetBufferPointer());
    return { data, data + (output->GetBufferSize() / 4) };
}

ShaderReflectionData ShaderCompiler::reflect_spirv(ShaderReflectionDesc *desc, eastl::span<u32> ir)
{
    ZoneScoped;

    constexpr static ShaderStage kStageMap[] = {
        [spv::ExecutionModel::ExecutionModelVertex] = ShaderStage::Vertex,
        [spv::ExecutionModel::ExecutionModelFragment] = ShaderStage::Pixel,
        [spv::ExecutionModel::ExecutionModelGLCompute] = ShaderStage::Compute,
        [spv::ExecutionModel::ExecutionModelTessellationControl] = ShaderStage::TessellationControl,
        [spv::ExecutionModel::ExecutionModelTessellationEvaluation] = ShaderStage::TessellationEvaluation,
    };

    spirv_cross::CompilerHLSL compiler(ir.data(), ir.size());
    auto first_entry = compiler.get_entry_points_and_stages()[0];
    auto resources = compiler.get_shader_resources();
    auto &entry_point = compiler.get_entry_point(first_entry.name, first_entry.execution_model);

    auto push_constants_id = resources.push_constant_buffers[0].id;
    auto push_constants_type_id = resources.push_constant_buffers[0].base_type_id;
    auto push_constant_ranges = compiler.get_active_buffer_ranges(push_constants_id);
    auto uniforms = resources.uniform_buffers;

    ShaderReflectionData result = {};
    result.m_compiled_stage = kStageMap[entry_point.model];
    result.m_entry_point = first_entry.name.data();

    for (auto &r : push_constant_ranges)
    {
        auto &name = compiler.get_member_name(push_constants_type_id, r.index);
        if (name == desc->m_descriptors_start_name.data())
            result.m_descriptor_start_offset = r.offset;

        result.m_push_constant_size += r.range;
    }

    if (result.m_compiled_stage == ShaderStage::Vertex && desc->m_reflect_vertex_layout)
    {
        u32 offset = 0;
        for (auto &input : resources.stage_inputs)
        {
            auto id = input.id;
            auto &type = compiler.get_type(input.type_id);
            u32 location = compiler.get_decoration(id, spv::DecorationLocation);

            Format format = to_vk_format(type);
            result.m_vertex_attribs.push_back(PipelineVertexAttribInfo(0, location, format, offset));
            offset += format_to_size(format);
        }
    }

    return result;
}

}  // namespace lr::Graphics