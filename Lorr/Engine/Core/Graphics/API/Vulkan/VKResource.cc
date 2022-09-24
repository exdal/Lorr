#include "VKResource.hh"

namespace lr::g
{
    void VKImage::Init(ImageDesc *pDesc, ImageData *pData)
    {
        ZoneScoped;

        m_TotalMips = pDesc->MipMapLevels;
        m_Format = pDesc->Format;
        m_Width = pData->Width;
        m_Height = pData->Height;
        m_DataSize = pData->DataLen;
    }

    void VKImage::Delete()
    {
        ZoneScoped;

        // TODO: destroy buffer
    }

    VKImage::~VKImage()
    {
        Delete();
    }

}  // namespace lr::g