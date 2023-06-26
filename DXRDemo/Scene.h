#pragma once

#include <memory>
#include "SceneObject.h"

namespace DXRDemo
{
    class Scene final
    {
    public:
        std::shared_ptr<SceneObject> RootSceneObject;

        inline void Update()
        {
            RootSceneObject->Update();
        }

        inline void Render()
        {
            RootSceneObject->Update();
        }
    };
}