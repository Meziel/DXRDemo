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

        template <typename T>
        inline bool ForEachComponent(const std::function<bool(T&)>& callback)
        {
            for (auto& component : Components)
            {
                T* componentType = dynamic_cast<T*>(component.get());
                if (componentType != nullptr)
                {
                    bool shouldReturn = callback(*componentType);
                    if (shouldReturn)
                    {
                        return true;
                    }
                }
            }

            for (const std::shared_ptr<GameObject>& child : Children)
            {
                if (child->ForEachComponent<T>(callback))
                {
                    return true;
                }
            }

            return false;
        }
    };

}
