// Created on Thursday May 11th 2023 by exdal
// Last modified on Saturday May 20th 2023 by exdal
#pragma once

#include "Crypt/FNV.hh"
#include "Memory/Allocator/LinearAllocator.hh"

namespace lm
{
union VariableData
{
    template<typename _Type>
    _Type As()
    {
        return (_Type)NumberI;
    }

    char *pString = nullptr;
    double NumberF;
    i64 NumberI;

    struct
    {
        u32 IsArray : 1;
        u32 ArrayOffset : 31;
        u32 ArrayElementCount;
    };
};

struct Variable
{
    u64 m_Hash = 0;
    VariableData m_Data = {};
};

struct Result;
struct Category
{
    eastl::string_view GetString(eastl::string_view name)
    {
        ZoneScoped;

        VariableData *pData = FindVariable(lr::Hash::FNV64String(name));
        if (pData)
            return pData->pString;
        return "";
    }

    template<typename _Type>
    _Type Get(eastl::string_view name)
    {
        ZoneScoped;

        VariableData *pData = FindVariable(lr::Hash::FNV64String(name));
        if (pData)
            return (_Type)pData->NumberI;
        return _Type{};
    }

    template<typename _Type>
    bool GetArray(eastl::string_view name, eastl::span<_Type> &array)
    {
        ZoneScoped;

        VariableData *pData = FindVariable(lr::Hash::FNV64String(name));
        if (!pData || !pData->IsArray)
            return false;

        VariableData *pVarData = (m_ArrayValues.begin() + pData->ArrayOffset);
        array = eastl::span<_Type>((_Type *)&pVarData->NumberI, (uptr)pData->ArrayElementCount);

        return true;
    }

    Variable &GetVariable(u32 id) { return m_Vars[id]; }
    VariableData *FindVariable(u64 hash)
    {
        ZoneScoped;

        for (auto &[curHash, data] : m_Vars)
            if (curHash == hash)
                return &data;

        return nullptr;
    }

    VariableData operator[](eastl::string_view varName)
    {
        ZoneScoped;

        VariableData *pData = FindVariable(lr::Hash::FNV64String(varName));
        return pData ? *pData : VariableData{};
    }

    u64 m_Hash = 0;
    u32 m_ParentCategory = 0;
    u32 m_CurrentVar = 0;

    eastl::vector<u32> m_SubCategories = {};
    eastl::vector<Variable> m_Vars = {};
    eastl::vector<VariableData> m_ArrayValues = {};
};

struct Result
{
    void PushCategory(const char *pName);
    void PushVar(const char *pName);
    void PushArray(const char *pName);

    void PopCategory();
    void PopVar();
    void PopArray();

    void PushString(const char *pVar);
    void PushNumber(i64 var);
    void PushDouble(double var);
    void PushBool(bool var);

    void PushError(u32 line, u32 column, const char *pMessage);

    char *AllocateString(eastl::string_view var);
    u32 AllocateCategory(const char *pName, u32 parent);
    Category &GetCategory(u32 id);
    Category &GetCategory(eastl::string_view categorySet);

    // Only valid for Global Category
    VariableData operator[](eastl::string_view varName)
    {
        VariableData *pData = m_GlobalCategory.FindVariable(lr::Hash::FNV64String(varName));
        return pData ? *pData : VariableData{};
    }

    lr::Memory::LinearAllocator m_StringAllocator = {};
    eastl::vector<Category> m_Categories = {};

    Category m_GlobalCategory = {};
    u32 m_CurrentCategory = 0;
};

void Init(Result *pLM, u64 initialMemory = 0xffff);
i32 ParseFromMemory(Result *pResult, eastl::string_view code);
i32 ParseFromFile(Result *pResult, eastl::string_view pathToFile);
}  // namespace lm