#include "Camera.hh"

namespace lr::Graphics
{
    void Camera3D::Init(Camera3DDesc *pDesc)
    {
        ZoneScoped;

        m_Position = XMVectorSet(pDesc->Position.x, pDesc->Position.y, pDesc->Position.z, 0.0);
        m_ViewDirection = XMVectorSet(pDesc->ViewDirection.x, pDesc->ViewDirection.y, pDesc->ViewDirection.z, 0.0);
        m_UpDirection = XMVectorSet(pDesc->UpDirection.x, pDesc->UpDirection.y, pDesc->UpDirection.z, 0.0);

        m_ViewSize = pDesc->ViewSize;
        m_FOV = pDesc->FOV;
        m_ZNear = pDesc->ZNear;
        m_ZFar = pDesc->ZFar;

        m_Aspect = m_ViewSize.x / m_ViewSize.y;
        m_RightDirection = XMVector3Normalize(XMVector3Cross(m_ViewDirection, XMVectorSet(1.0, 0.0, 0.0, 0.0)));
    }

    void Camera3D::CalculateView()
    {
        ZoneScoped;

        XMVECTOR posDir = m_Position + m_ViewDirection;
        m_View = XMMatrixLookAtLH(m_Position, posDir, m_UpDirection);
    }

    void Camera3D::CalculateProjection()
    {
        ZoneScoped;

        m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FOV), m_Aspect, m_ZNear, m_ZFar);
    }

    void Camera3D::SetPosition(const XMFLOAT3 &position)
    {
        ZoneScoped;

        XMVectorSet(position.x, position.y, position.z, 0.0);
    }

    void Camera3D::SetSize(const XMFLOAT2 &viewSize)
    {
        ZoneScoped;

        m_ViewSize = viewSize;
    }

    void Camera3D::SetDirectionAngle(XMFLOAT2 angle)
    {
        ZoneScoped;

        m_DirectionAngle = angle;

        // angle y = pitch, x = yaw

        angle.y = eastl::clamp(angle.y, -89.0f, 89.0f);

        XMFLOAT2 radDirection(XMConvertToRadians(angle.x), XMConvertToRadians(angle.y));
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(radDirection.y, radDirection.x, 0.0);

        m_ViewDirection = XMVector3Normalize(XMVector3TransformCoord(XMVectorSet(0.0, 0.0, 1.0, 0.0), rotationMatrix));

        rotationMatrix = XMMatrixRotationY(radDirection.x);
        m_RightDirection = XMVector3TransformCoord(XMVectorSet(1.0, 0.0, 0.0, 0.0), rotationMatrix);
        m_UpDirection = XMVector3TransformCoord(m_UpDirection, rotationMatrix);
    }

    void Camera3D::Update(f32 deltaTime, f32 movementSpeed)
    {
        ZoneScoped;

        CalculateProjection();
        CalculateView();
    }

    void Camera2D::Init(Camera2DDesc *pDesc)
    {
        ZoneScoped;

        m_Position = XMVectorSet(pDesc->Position.x, pDesc->Position.y, 0.0, 1.0);
        m_ViewSize = pDesc->ViewSize;

        m_ZNear = pDesc->ZNear;
        m_ZFar = pDesc->ZFar;
    }

    void Camera2D::CalculateView()
    {
        ZoneScoped;

        XMMATRIX translation = XMMatrixIdentity();
        translation = XMMatrixMultiply(translation, XMMatrixTranslationFromVector(m_Position));

        XMMATRIX rotation = XMMatrixIdentity();
        rotation = XMMatrixMultiply(rotation, XMMatrixRotationAxis(XMVectorSet(0.0, 0.0, 1.0, 0.0), 0.0));

        m_View = XMMatrixInverse(nullptr, XMMatrixMultiply(translation, rotation));
    }

    void Camera2D::SetPosition(const XMFLOAT2 &position)
    {
        ZoneScoped;

        XMVectorSet(position.x, position.y, 0.0, 1.0);
    }

    void Camera2D::SetSize(const XMFLOAT2 &viewSize)
    {
        ZoneScoped;

        m_ViewSize = viewSize;
    }

    void Camera2D::Update(f32 deltaTime, f32 movementSpeed)
    {
        ZoneScoped;

        CalculateView();
    }

}  // namespace lr::Graphics