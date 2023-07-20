#pragma once

namespace DXRDemo
{
    class GameObject;

    class Component
    {
    public:
        GameObject* Parent = nullptr;
        virtual ~Component() = default;
        virtual void Update(double deltaTime) {};
    };
}
