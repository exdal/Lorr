// Created on Sunday October 9th 2022 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

namespace lr::Graphics::Limits
{
constexpr usize MaxPushConstants = 8;
constexpr usize MaxVertexAttribs = 8;
constexpr usize MaxColorAttachments = 8;
constexpr usize MaxFrameCount = 8;
}  // namespace lr::Graphics::Limits

/// PIPELINE
// #define LR_MAX_DESCRIPTOR_SETS_PER_PIPELINE 8
// #define LR_MAX_PUSH_CONSTANTS_PER_PIPELINE 8
// #define LR_MAX_BINDINGS_PER_LAYOUT 8
// #define LR_MAX_STATIC_SAMPLERS_PER_PIPELINE 8
// #define LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE 8
// #define LR_MAX_COLOR_ATTACHMENT_PER_PASS 8

/// SWAPCHAIN
// #define LR_MAX_FRAME_COUNT 3