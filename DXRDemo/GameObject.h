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

        GameObject* Parent = nullptr;
        Transform Transform;
        std::vector<std::shared_ptr<GameObject>> Children;
        std::vector<std::shared_ptr<Component>> Components;

        virtual void Update(double deltaTime);

        void AddChild(const std::shared_ptr<GameObject>& child);
        void AddComponent(const std::shared_ptr<Component>& component);

        inline bool ForEachChild(const std::function<bool(GameObject&, std::size_t)>& callback)
        {
            std::size_t index = 0;
            return ForEachChildHelper(callback, index);
        }

        inline bool ForEachChildHelper(const std::function<bool(GameObject&, std::size_t)>& callback, std::size_t& index)
        {
            for (const std::shared_ptr<GameObject>& child : Children)
            {
                bool shouldReturn = callback(*child, index);
                if (shouldReturn)
                {
                    return true;
                }
                ++index;
            }

            for (const std::shared_ptr<GameObject>& child : Children)
            {
                if (child->ForEachChildHelper(callback, index))
                {
                    return true;
                }
            }

            return false;
        }

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
