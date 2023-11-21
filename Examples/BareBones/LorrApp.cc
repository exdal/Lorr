// Created on Thursday January 26th 2023 by exdal
// Last modified on Tuesday August 29th 2023 by exdal

#include "LorrApp.hh"

void LorrApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    create(desc);
}

void LorrApp::shutdown()
{
    ZoneScoped;
}

void LorrApp::poll(f32 deltaTime)
{
    ZoneScoped;
}
