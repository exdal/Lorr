// Created on Thursday January 26th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#include "LorrApp.hh"

void LorrApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    PreInit(desc);
}

void LorrApp::Shutdown()
{
    ZoneScoped;
}

void LorrApp::Poll(f32 deltaTime)
{
    ZoneScoped;
}
