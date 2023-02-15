#include "BaseShader.hh"

#include "STL/Memory.hh"

namespace lr::Graphics
{
    static eastl::wstring_view GetFullShaderModel(ShaderStage type)
    {
        ZoneScoped;

        eastl::wstring_view VSTag = L"vs_6_1";
        eastl::wstring_view PSTag = L"ps_6_1";
        eastl::wstring_view CSTag = L"cs_6_1";
        eastl::wstring_view DSTag = L"ds_6_1";
        eastl::wstring_view HSTag = L"hs_6_1";

        switch (type)
        {
            case LR_SHADER_STAGE_VERTEX:
                return VSTag;
                break;
            case LR_SHADER_STAGE_PIXEL:
                return PSTag;
                break;
            case LR_SHADER_STAGE_COMPUTE:
                return CSTag;
                break;
            case LR_SHADER_STAGE_HULL:
                return HSTag;
                break;
            case LR_SHADER_STAGE_DOMAIN:
                return DSTag;
                break;
            default:
                break;
        }

        return nullptr;
    }

    IDxcBlob *ShaderCompiler::CompileFromBuffer(ShaderCompileDesc *pDesc, BufferReadStream &buf)
    {
        ZoneScoped;

        // TODO: Some pointers here don't get released after `hr < 0`. Implement scoped pointers.

        ls::scoped_comptr<IDxcCompiler3> pCompiler;
        DxcCreateInstance(CLSID_DxcCompiler, LS_IID_PTR(pCompiler));

        DxcBuffer shaderBuffer = {};
        shaderBuffer.Ptr = buf.GetData();
        shaderBuffer.Size = buf.GetLength();
        shaderBuffer.Encoding = DXC_CP_ACP;

        eastl::vector<LPCWSTR> strFlags;

        if (pDesc->m_Flags & LR_SHADER_FLAG_USE_SPIRV)
        {
            strFlags.push_back(L"-spirv");
        }
        else
        {
            strFlags.push_back(L"-Wno-vk-ignored-features");
            strFlags.push_back(L"-Wno-ignored-attributes");
            if (pDesc->m_Flags & LR_SHADER_FLAG_USE_DEBUG)
                strFlags.push_back(DXC_ARG_DEBUG);
            if (pDesc->m_Flags & LR_SHADER_FLAG_SKIP_VALIDATION)
                strFlags.push_back(DXC_ARG_SKIP_VALIDATION);
        }

        if (pDesc->m_Flags & LR_SHADER_FLAG_WARNINGS_ARE_ERRORS)
            strFlags.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
        if (pDesc->m_Flags & LR_SHADER_FLAG_MATRIX_ROW_MAJOR)
            strFlags.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);
        if (pDesc->m_Flags & LR_SHADER_FLAG_SKIP_OPTIMIZATION)
            strFlags.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
        else
            strFlags.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);

        ls::scoped_comptr<IDxcCompilerArgs> pCompilerArgs;
        DxcCreateInstance(CLSID_DxcCompilerArgs, LS_IID_PTR(pCompilerArgs));

        ls::scoped_comptr<IDxcUtils> pUtils;
        DxcCreateInstance(CLSID_DxcUtils, LS_IID_PTR(pUtils));

        DxcDefine pDefines[ShaderCompileDesc::kMaxDefinceCount] = {};

        for (u32 i = 0; i < pDesc->m_DefineCount; i++)
        {
            pDefines[i].Name = pDesc->m_Defines[i].m_Name.data();
            pDefines[i].Value = pDesc->m_Defines[i].m_Value.data();
        }

        pUtils->BuildArguments(
            pDesc->m_PathOpt.data(),
            L"main",
            GetFullShaderModel(pDesc->m_Type).data(),
            &strFlags[0],
            strFlags.size(),
            pDefines,
            pDesc->m_DefineCount,
            pCompilerArgs.get_address());

        ls::scoped_comptr<IDxcIncludeHandler> pIncludeHandler;
        pUtils->CreateDefaultIncludeHandler(pIncludeHandler.get_address());

        ls::scoped_comptr<IDxcResult> pResult;
        HRESULT hr = pCompiler->Compile(
            &shaderBuffer,
            pCompilerArgs->GetArguments(),
            pCompilerArgs->GetCount(),
            pIncludeHandler.get(),
            LS_IID_PTR(pResult));

        ls::scoped_comptr<IDxcBlobUtf8> pError;
        pResult->GetOutput(DXC_OUT_ERRORS, LS_IID_PTR(pError), nullptr);

        if (pError.get() && pError->GetBufferPointer())
            LOG_ERROR("Shader compiler message: {}", (const char *)pError->GetBufferPointer());

        if (hr < 0)
        {
            return nullptr;
        }

        IDxcBlob *pCode = nullptr;
        pResult->GetResult(&pCode);

        return pCode;
    }

}  // namespace lr::Graphics