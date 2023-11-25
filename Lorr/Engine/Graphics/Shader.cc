#include "Shader.hh"

#include <atlbase.h>
#include <dxc/dxcapi.h>
#include <spirv_hlsl.hpp>

namespace lr::Graphics
{
Format to_vk_format(const spirv_cross::SPIRType &type)
{
    using namespace spirv_cross;
    constexpr static Format float_formats[] = { [0] = Format::Float, [1] = Format::Vec2, [2] = Format::Vec3, [3] = Format::Vec4 };
    constexpr static Format int_formats[] = { [0] = Format::Int, [1] = Format::Vec2I, [2] = Format::Vec3I, [3] = Format::Vec4I };
    constexpr static Format uint_formats[] = { [0] = Format::UInt, [1] = Format::Vec2U, [2] = Format::Vec3U, [3] = Format::Vec4U };
    constexpr static auto format_map[] = { [SPIRType::Float] = float_formats, [SPIRType::Int] = int_formats, [SPIRType::UInt] = uint_formats };

    if (type.columns != 1)
        return Format::Unknown;  // Skip matrix types

    return format_map[type.basetype][type.vecsize];
}

eastl::vector<u32> ShaderCompiler::compile_shader(ShaderCompileDesc *desc)
{
    ZoneScoped;

    constexpr static eastl::wstring_view kHLSLStageMap[] = {
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

    eastl::vector<LPCWSTR> args = {};
    args.push_back(L"-E");
    args.push_back(L"main");
    args.push_back(L"-spirv");
    args.push_back(L"-fspv-target-env=vulkan1.3");
    args.push_back(L"-fvk-use-gl-layout");
    args.push_back(L"-no-warnings");
    // args.push_back(L"-I");
    // args.push_back(L"path/to/working/dir");
    args.push_back(L"-T");
    args.push_back(kHLSLStageMap[(u32)desc->m_target_stage].data());

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

    spirv_cross::CompilerHLSL compiler(ir.data(), ir.size_bytes());
    auto first_entry = compiler.get_entry_points_and_stages()[0];
    auto resources = compiler.get_shader_resources();
    auto &entry_point = compiler.get_entry_point(first_entry.name, first_entry.execution_model);
    auto push_constants_id = resources.push_constant_buffers[0].id;
    auto push_constants_type_id = resources.push_constant_buffers[0].base_type_id;
    auto push_constant_ranges = compiler.get_active_buffer_ranges(push_constants_id);

    ShaderReflectionData result = {};
    result.m_compiled_stage = kStageMap[entry_point.model];
    for (auto &range : push_constant_ranges)
    {
        auto &name = compiler.get_member_name(push_constants_type_id, range.index);
        if (name == desc->m_descriptors_start_name.data())
            result.m_descriptor_start_offset = range.offset;

        result.m_push_constant_size += range.range;
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