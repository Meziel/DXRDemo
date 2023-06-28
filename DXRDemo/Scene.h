#pragma once

#include <memory>
#include "GameObject.h"

namespace DXRDemo
{
    class Scene final
    {
    public:
        std::shared_ptr<GameObject> RootSceneObject;

        inline void Update(double deltaTime)
        {
            RootSceneObject->Update(deltaTime);
        }

        inline void Render()
        {
            RootSceneObject->Render();
        }
    };
}