#include "Parser.hh"

#include "Crypt/FNV.hh"
#include "IO/FileStream.hh"

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
    u32 offset = m_ArrayAllocator.size();
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
    char *pString = AllocateString(pVar);
    if (data.IsArray)
    {
        data.ArrayElementCount++;
        m_ArrayAllocator.push_back({ .pString = pString });
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
        m_ArrayAllocator.push_back({ .NumberI = var });
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
        m_ArrayAllocator.push_back({ .NumberF = var });
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

    free((void *)pMessage);
}

char *Result::AllocateString(const char *pSrc)
{
    ZoneScoped;

    char *pAllocated = (char *)m_StringAllocator.Allocate(strlen(pSrc) + 1);
    strcpy(pAllocated, pSrc);

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

    state = lm__scan_string((char *)code.data(), scanner);
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

    lr::FileStream scriptFile(pathToFile, false);
    if (!scriptFile.IsOK())
    {
        LOG_ERROR("Failed to load '{}'.", pathToFile);
        return -2;
    }

    const char *pScript = scriptFile.ReadAll<char>();
    u32 size = scriptFile.Size();
    scriptFile.Close();

    i32 result = lm::ParseFromMemory(pResult, eastl::string_view(pScript, size));

    free((void *)pScript);

    return result;
}