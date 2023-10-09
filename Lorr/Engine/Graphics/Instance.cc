#include "Instance.hh"

namespace lr::Graphics
{
bool Instance::Init(InstanceDesc *pDesc)
{
    u32 availExtensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensions, nullptr);
    VkExtensionProperties *pExtensions = new VkExtensionProperties[availExtensions];
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensions, pExtensions);

    for (eastl::string_view extension : pDesc->m_Extensions)
    {
        bool found = false;
        for (u32 i = 0; i < availExtensions; i++)
        {
            VkExtensionProperties &properties = pExtensions[i];
            if (properties.extensionName == extension)
            {
                found = true;
                break;
            }
        }

        LOG_INFO("\t{}, {}", extension, found);
        if (found)
            continue;
        LOG_ERROR("Following extension is not found in this instance: {}", extension);
        return false;
    }

    delete[] pExtensions;

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = pDesc->m_AppName.data(),
        .applicationVersion = pDesc->m_AppVersion,
        .pEngineName = pDesc->m_EngineName.data(),
        .engineVersion = pDesc->m_EngineVersion,
        .apiVersion = pDesc->m_APIVersion,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = (u32)pDesc->m_Extensions.size(),
        .ppEnabledExtensionNames = pDesc->m_Extensions.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
    };
    vkCreateInstance(&createInfo, nullptr, &m_pInstance);

    return true;
}

}  // namespace lr::Graphics