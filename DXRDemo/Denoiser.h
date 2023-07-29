#pragma once

#include <OpenImageDenoise/oidn.hpp>
#include <d3d12.h>
#include "Utilities.h"

namespace DXRDemo
{
    class Denoiser
    {
    public:
        Denoiser(ID3D12Device5* device, ID3D12Resource* resource)
        {
            _inputImageHandler = nullptr;
            ThrowIfFailed(device->CreateSharedHandle(resource, NULL, GENERIC_ALL, NULL, &_inputImageHandler));

            // Create an Open Image Denoise device
            _device = oidn::newDevice(oidn::DeviceType::CUDA); // CPU or GPU if available
            _device.commit();

            const char* errorMessage;
            if (_device.getError(errorMessage) != oidn::Error::None)
            {
                OutputDebugStringA(errorMessage);
                //throw std::runtime_error(errorMessage);
            }

            // Create buffers for input/output images accessible by both host (CPU) and device (CPU/GPU)
            D3D12_RESOURCE_DESC desc = resource->GetDesc();
            oidn::BufferRef colorBuf =  _device.newBuffer(oidn::ExternalMemoryTypeFlag::D3D12Resource, _inputImageHandler, nullptr, desc.Width * desc.Height * 4);

            // Create a filter for denoising a beauty (color) image using optional auxiliary images too
            // This can be an expensive operation, so try no to create a new filter for every image!
            oidn::FilterRef _filter = _device.newFilter("RT"); // generic ray tracing filter
            _filter.setImage("color", colorBuf, oidn::Format::Float4, desc.Width, desc.Height); // beauty
            //filter.setImage("albedo", albedoBuf, oidn::Format::Float3, width, height); // auxiliary
            //filter.setImage("normal", normalBuf, oidn::Format::Float3, width, height); // auxiliary
            _filter.setImage("output", colorBuf, oidn::Format::Float4, desc.Width, desc.Height); // denoised beauty
            _filter.set("hdr", false); // beauty image is HDR
            _filter.commit();

            // Fill the input image buffers
            //float* colorPtr = (float*)colorBuf.getData();
        }

        ~Denoiser()
        {
            if (_inputImageHandler != nullptr)
            {
                CloseHandle(_inputImageHandler);
            }
        }

        void Denoise()
        {
            _filter.execute();

            const char* errorMessage;
            if (_device.getError(errorMessage) != oidn::Error::None)
            {
                OutputDebugStringA(errorMessage);
                //throw std::runtime_error(errorMessage);
            }
        }

    private:
        oidn::DeviceRef _device;
        oidn::FilterRef _filter;
        HANDLE _inputImageHandler;
    };
}
