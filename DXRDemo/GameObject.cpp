#include "GameObject.h"

namespace DXRDemo
{
    void GameObject::Update(double deltaTime)
    {
        for (const std::shared_ptr<Component>& component : Components)
        {
            component->Update(deltaTime);
        }

        for (const std::shared_ptr<GameObject>& child : Children)
        {
            child->Update(deltaTime);
        }
    }

    void GameObject::AddChild(const std::shared_ptr<GameObject>& child)
    {
        Children.push_back(child);
        child->Parent = this;
    }

    void GameObject::AddComponent(const std::shared_ptr<Component>& component)
    {
        Components.push_back(component);
        component->Parent = this;
    }
}