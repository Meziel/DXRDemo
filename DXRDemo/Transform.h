#pragma once

#include <directxtk/SimpleMath.h>
#include <combaseapi.h>
#include <dxcapi.h>
#include <d3d12.h>
#include "framework.h"

struct Transform final
{
    DirectX::SimpleMath::Vector3 Position;
    DirectX::SimpleMath::Vector3 Scale = { 1.0f, 1.0f, 1.0f };
    DirectX::SimpleMath::Quaternion Rotation;

    DirectX::XMMATRIX ModelMatrix = DirectX::XMMatrixIdentity();
    Microsoft::WRL::ComPtr<ID3D12Resource> MvpBuffer;
    inline void UpdateModelMatrix()
    {
        ModelMatrix = XMMatrixAffineTransformation(
            Scale,
            DirectX::SimpleMath::Vector3(0, 0, 0),
            Rotation,
            Position);
    }
};
