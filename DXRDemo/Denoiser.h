#pragma once

#include <OpenImageDenoise/oidn.hpp>
#include <d3d12.h>
#include "Utilities.h"

namespace DXRDemo
{
    class Denoiser
    {
    public:
        Denoiser(std::size_t width, std::size_t height) :
            _width(width),
            _height(height)
        {

            //_inputImageHandler = nullptr;
            //ThrowIfFailed(device->CreateSharedHandle(resource, NULL, GENERIC_ALL, NULL, &_inputImageHandler));

            // Create an Open Image Denoise device
            _device = oidn::newDevice(oidn::DeviceType::Default); // CPU or GPU if available
            _device.commit();

            const char* errorMessage;
            if (_device.getError(errorMessage) != oidn::Error::None)
            {
                OutputDebugStringA(errorMessage);
                throw std::runtime_error(errorMessage);
            }

            colorBuf = _device.newBuffer(_width * _height * 3 * sizeof(float));

            // Create a filter for denoising a beauty (color) image using optional auxiliary images too
            // This can be an expensive operation, so try no to create a new filter for every image!
            _filter = _device.newFilter("RT"); // generic ray tracing filter
            _filter.setImage("color", colorBuf, oidn::Format::Float3, _width, _height); // beauty
            //filter.setImage("albedo", albedoBuf, oidn::Format::Float3, width, height); // auxiliary
            //filter.setImage("normal", normalBuf, oidn::Format::Float3, width, height); // auxiliary
            _filter.setImage("output", colorBuf, oidn::Format::Float3, _width, _height); // denoised beauty
            _filter.set("hdr", true); // beauty image is HDR
            _filter.commit();

            // Fill the input image buffers
            //float* colorPtr = (float*)colorBuf.getData();
        }

        ~Denoiser()
        {
            //if (_inputImageHandler != nullptr)
            //{
            //    CloseHandle(_inputImageHandler);
            //}
        }

        void Denoise(void* image, void* output)
        {
            std::byte* rgba8Image = reinterpret_cast<std::byte*>(image);
            std::byte* rgba8Output = reinterpret_cast<std::byte*>(output);
            float* colorPtr = reinterpret_cast<float*>(colorBuf.getData());

            for (int x = 0; x < _width; ++x)
            {
                for (int y = 0; y < _height; ++y)
                {
                    *colorPtr = static_cast<float>(*(rgba8Image));
                    ++colorPtr;
                    ++rgba8Image;

                    *colorPtr = static_cast<float>(*(rgba8Image));
                    ++colorPtr;
                    ++rgba8Image;

                    *colorPtr = static_cast<float>(*(rgba8Image));
                    ++colorPtr;
                    ++rgba8Image;

                    ++rgba8Image;
                }
            }

            _filter.execute();

            const char* errorMessage;
            if (_device.getError(errorMessage) != oidn::Error::None)
            {
                OutputDebugStringA(errorMessage);
                throw std::runtime_error(errorMessage);
            }

            colorPtr = reinterpret_cast<float*>(colorBuf.getData());
            for (int x = 0; x < _width; ++x)
            {
                for (int y = 0; y < _height; ++y)
                {
                    *rgba8Output = static_cast<std::byte>(std::min(*(colorPtr), 255.0f));
                    ++colorPtr;
                    ++rgba8Output;

                    *rgba8Output = static_cast<std::byte>(std::min(*(colorPtr), 255.0f));
                    ++colorPtr;
                    ++rgba8Output;

                    *rgba8Output = static_cast<std::byte>(std::min(*(colorPtr), 255.0f));
                    ++colorPtr;
                    ++rgba8Output;

                    *rgba8Output = static_cast<std::byte>(255);
                    ++rgba8Output;
                }
            }
        }

    private:
        std::size_t _width;
        std::size_t _height;
        oidn::BufferRef colorBuf;
        //oidn::BufferRef outputBuf;
        oidn::DeviceRef _device;
        oidn::FilterRef _filter;
        //HANDLE _inputImageHandler;
    };
}
