#pragma once

#include <directxtk/SimpleMath.h>

struct Transform final
{
    DirectX::SimpleMath::Vector3 Position;
    DirectX::SimpleMath::Vector3 Scale = { 1.0f, 1.0f, 1.0f };
    DirectX::SimpleMath::Quaternion Rotation;
};
