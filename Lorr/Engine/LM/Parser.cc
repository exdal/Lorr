// Created on Thursday May 11th 2023 by exdal
// Last modified on Friday June 30th 2023 by exdal
#include "Parser.hh"

#include "Crypt/FNV.hh"
#include "STL/String.hh"
#include "IO/File.hh"

#include "lm_bison.hh"
#include "lm_flex.hh"

namespace lm
{
void Result::PushCategory(const char *pName)
{
    ZoneScoped;

    u32 parentID = m_CurrentCategory;
    u32 category = AllocateCategory(pName, m_CurrentCategory);
    GetCategory(parentID).m_SubCategories.push_back(category);

    m_CurrentCategory = category;

    free((void *)pName);
}

void Result::PushVar(const char *pName)
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    category.m_CurrentVar = category.m_Vars.size();
    category.m_Vars.push_back({ .m_Hash = lr::Hash::FNV64String(pName) });

    free((void *)pName);
}

void Result::PushArray(const char *pName)
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    u32 offset = category.m_ArrayValues.size();
    category.m_CurrentVar = category.m_Vars.size();
    category.m_Vars.push_back({
        .m_Hash = lr::Hash::FNV64String(pName),
        .m_Data = { .IsArray = true, .ArrayOffset = offset, .ArrayElementCount = 0 },
    });

    free((void *)pName);
}

void Result::PopCategory()
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    m_CurrentCategory = category.m_ParentCategory;
}

void Result::PopVar()
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    category.m_CurrentVar--;
}

void Result::PopArray()
{
    ZoneScoped;
}

void Result::PushString(const char *pVar)
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    VariableData &data = category.GetVariable(category.m_CurrentVar).m_Data;
    char *pString = AllocateString(ls::TrimQuotes(pVar));
    if (data.IsArray)
    {
        data.ArrayElementCount++;
        category.m_ArrayValues.push_back({ .pString = pString });
    }
    else
    {
        data.pString = pString;
    }

    free((void *)pVar);
}

void Result::PushNumber(i64 var)
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    VariableData &data = category.GetVariable(category.m_CurrentVar).m_Data;
    if (data.IsArray)
    {
        data.ArrayElementCount++;
        category.m_ArrayValues.push_back({ .NumberI = var });
    }
    else
    {
        data.NumberI = var;
    }
}

void Result::PushDouble(double var)
{
    ZoneScoped;

    Category &category = GetCategory(m_CurrentCategory);
    VariableData &data = category.GetVariable(category.m_CurrentVar).m_Data;
    if (data.IsArray)
    {
        data.ArrayElementCount++;
        category.m_ArrayValues.push_back({ .NumberF = var });
    }
    else
    {
        data.NumberF = var;
    }
}

void Result::PushBool(bool var)
{
    ZoneScoped;

    PushNumber(var);
}

void Result::PushError(u32 line, u32 column, const char *pMessage)
{
    ZoneScoped;

    LOG_ERROR("Failed to real LM: ({}:{}): {}", line, column, pMessage);
}

char *Result::AllocateString(eastl::string_view var)
{
    ZoneScoped;

    char *pAllocated = (char *)m_StringAllocator.Allocate(var.length() + 1);
    memcpy(pAllocated, var.data(), var.length());
    pAllocated[var.length() + 1] = '\0';

    return pAllocated;
}

u32 Result::AllocateCategory(const char *pName, u32 parent)
{
    ZoneScoped;

    m_Categories.push_back({ .m_Hash = lr::Hash::FNV64String(pName), .m_ParentCategory = parent });

    return m_Categories.size();
}

Category &Result::GetCategory(u32 id)
{
    ZoneScoped;

    if (id == 0)
        return m_GlobalCategory;

    return m_Categories[id - 1];
}

Category &Result::GetCategory(eastl::string_view categorySet)
{
    ZoneScoped;

    uptr namePos = -1;
    uptr lastPos = 0;
    Category *pFoundCategory = &m_GlobalCategory;
    eastl::string_view currentCategory = categorySet;

    do
    {
        namePos = categorySet.find('.', lastPos);
        currentCategory = categorySet.substr(lastPos, namePos - lastPos);

        if (namePos != -1)
            lastPos = namePos + 1;

        u64 hash = lr::Hash::FNV64String(currentCategory);
        for (u32 &categoryID : pFoundCategory->m_SubCategories)
        {
            Category &category = GetCategory(categoryID);
            if (category.m_Hash == hash)
            {
                pFoundCategory = &category;
            }
        }

    } while (namePos != -1);

    return *pFoundCategory;
}

}  // namespace lm

void lm::Init(Result *pLM, u64 initialMemory)
{
    ZoneScoped;

    pLM->m_StringAllocator.Init({ .m_DataSize = initialMemory });
}

i32 lm::ParseFromMemory(Result *pResult, eastl::string_view code)
{
    ZoneScoped;

    yyscan_t scanner;
    YY_BUFFER_STATE state;

    if (lm_lex_init(&scanner))
    {
        LOG_ERROR("LM: Failed to initialize LM lexer!");
        return -1;
    }

    state = lm__scan_bytes((char *)code.data(), code.size(), scanner);
    if (lm_parse(pResult, scanner) != 0)
    {
        LOG_ERROR("LM: Failed to parse LM buffer!");
        return 0;
    }

    lm__delete_buffer(state, scanner);
    lm_lex_destroy(scanner);

    return 1;
}

i32 lm::ParseFromFile(Result *pResult, eastl::string_view pathToFile)
{
    ZoneScoped;

    lr::FileView scriptFile(pathToFile);
    if (!scriptFile.IsOK())
    {
        LOG_ERROR("Failed to load '{}'.", pathToFile);
        return -2;
    }

    i32 result = lm::ParseFromMemory(pResult, scriptFile.GetAsString());

    return result;
}