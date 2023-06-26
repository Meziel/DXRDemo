#pragma once

#include <vector>
#include <memory>
#include "Transform.h"
#include "Component.h"

namespace DXRDemo
{
    class GameObject
    {
    public:
        virtual ~GameObject() = default;

        Transform Transform;
        std::vector<std::shared_ptr<GameObject>> Children;
        std::vector<std::shared_ptr<Component>> Components;

        virtual void Update(double deltaTime);
        virtual void Render();
    };

}
