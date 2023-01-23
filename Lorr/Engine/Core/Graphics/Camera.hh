//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

namespace lr::Graphics
{
    struct Camera3DDesc
    {
        XMFLOAT3 m_Position;
        XMFLOAT2 m_ViewSize;
        XMFLOAT3 m_ViewDirection;
        XMFLOAT3 m_UpDirection;

        f32 m_FOV;
        f32 m_ZNear;
        f32 m_ZFar;
    };

    struct Camera3D
    {
        void Init(Camera3DDesc *pDesc);

        void CalculateView();
        void CalculateProjection();

        void SetPosition(const XMFLOAT3 &position);
        void SetSize(const XMFLOAT2 &viewSize);
        void SetDirectionAngle(XMFLOAT2 angle);

        void Update(f32 deltaTime, f32 movementSpeed);

        XMMATRIX m_View = {};
        XMMATRIX m_Projection = {};

        XMVECTOR m_Position = {};
        XMFLOAT2 m_ViewSize = {};

        XMVECTOR m_ViewDirection = {};
        XMVECTOR m_UpDirection = {};
        XMVECTOR m_RightDirection = {};

        f32 m_FOV = 0.0;
        f32 m_Aspect = 0.0;
        f32 m_ZNear = 0.0;
        f32 m_ZFar = 0.0;

        XMFLOAT2 m_DirectionAngle = {};
    };

    struct Camera2DDesc
    {
        XMFLOAT2 m_Position;
        XMFLOAT2 m_ViewSize;

        f32 m_ZNear = 0.0;
        f32 m_ZFar = 0.0;
    };

    struct Camera2D
    {
        void Init(Camera2DDesc *pDesc);

        void CalculateView();

        void SetPosition(const XMFLOAT2 &position);
        void SetSize(const XMFLOAT2 &viewSize);

        void Update(f32 deltaTime, f32 movementSpeed);

        XMMATRIX m_View = {};
        XMMATRIX m_Projection = {};

        XMVECTOR m_Position = {};
        XMFLOAT2 m_ViewSize = {};

        f32 m_ZNear = 0.0;
        f32 m_ZFar = 0.0;
    };

}  // namespace lr::Graphics