#include "GameObject.h"

namespace DXRDemo
{
    void GameObject::Update(double deltaTime)
    {
        for (const std::shared_ptr<GameObject>& child : Children)
        {
            child->Update(deltaTime);
        }
    }
    void GameObject::Render()
    {
        for (const std::shared_ptr<GameObject>& child : Children)
        {
            child->Render();
        }
    }
}