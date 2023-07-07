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
        inline bool ForEachComponent(const std::function<bool(T&, std::size_t)>& callback)
        {
            std::size_t index = 0;
            return ForEachComponentHelper(callback, index);
        }

        template <typename T>
        inline bool ForEachComponentHelper(const std::function<bool(T&, std::size_t)>& callback, std::size_t& index)
        {
            for (auto& component : Components)
            {
                T* componentType = dynamic_cast<T*>(component.get());
                if (componentType != nullptr)
                {
                    bool shouldReturn = callback(*componentType, index);
                    if (shouldReturn)
                    {
                        return true;
                    }
                    ++index;
                }
            }

            for (const std::shared_ptr<GameObject>& child : Children)
            {
                if (child->ForEachComponentHelper<T>(callback, index))
                {
                    return true;
                }
            }

            return false;
        }
    };

}
