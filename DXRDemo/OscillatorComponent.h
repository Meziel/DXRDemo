#pragma once

#include "Component.h"
#include <vector>
#include <memory>

namespace DXRDemo
{
    class OscillatorComponent : public Component
    {
    public:

        float Radius = 1;
        float Speed = 1;

        virtual void Update(double deltaTime)
        {
            if (Parent != nullptr)
            {
                angle += Speed * deltaTime;
                angle = fmodf(angle, 2 * DirectX::XM_PI);
                
                Parent->Transform.Position.x = Radius * cos(angle);
                Parent->Transform.Position.z = Radius * sin(angle);
            }
        };

    private:
        float angle = 0;
    };
}