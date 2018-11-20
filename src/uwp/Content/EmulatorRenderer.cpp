#include "pch.h"
#include "EmulatorRenderer.h"

#include "..\Common\DirectXHelper.h"

#include <GPU.h>
#include <Config.h>

#include <SimpleMath.h>

using namespace uwp;

using namespace DirectX;
using namespace Windows::Foundation;

using namespace Windows::UI::Xaml;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
EmulatorRenderer::EmulatorRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
    const std::shared_ptr<uwp::Emulator> &emulatorInst) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources),
    m_emulator(emulatorInst)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void EmulatorRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.7f, 1.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

void EmulatorRenderer::UpdateScreen()
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    D3D11_MAPPED_SUBRESOURCE screen1Data;
    context->Map(m_screen1Texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &screen1Data);

    BYTE* mappedData = reinterpret_cast<BYTE*>(screen1Data.pData);
    u8 *buffer = reinterpret_cast<u8*>(GPU::Framebuffer);

    for (UINT i = 0; i < 192; ++i)
    {
        memcpy(mappedData, buffer, 256 * 4);
        mappedData += screen1Data.RowPitch;
        buffer += 256 * 4;
    }

    context->Unmap(m_screen1Texture.Get(), 0);

    D3D11_MAPPED_SUBRESOURCE screen2Data;
    context->Map(m_screen2Texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &screen2Data);

    mappedData = reinterpret_cast<BYTE*>(screen2Data.pData);
    buffer = reinterpret_cast<u8*>(&GPU::Framebuffer[256 * 192]);

    for (UINT i = 0; i < 192; ++i)
    {
        memcpy(mappedData, buffer, 256 * 4);
        mappedData += screen2Data.RowPitch;
        buffer += 256 * 4;
    }

    context->Unmap(m_screen2Texture.Get(), 0);

    m_loadingComplete = true;
    m_emulator->NotifyUploadDone();
}

// Check and update the NDS framebuffer if possible
void EmulatorRenderer::Update(DX::StepTimer const& timer)
{
    if (m_emulator->IsUploadPending())
    {
        m_loadingComplete = false;
        UpdateScreen();
    }
}

void EmulatorRenderer::StartTracking()
{
	m_tracking = true;
}

void EmulatorRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
	}
}

void EmulatorRenderer::StopTracking()
{
	m_tracking = false;
}

void EmulatorRenderer::Render()
{
	// If there is a uploading, wait for it to be done
    if (!m_loadingComplete)
	{
		return;
	}

    m_batch->Begin();

    bool horizontal = false;
    DirectX::SimpleMath::Vector2 scaleVec{1};
    
    if (Config::ScreenLayout == 2)
    {
        horizontal = true;

        if (Window::Current)
        {
            scaleVec = DirectX::SimpleMath::Vector2(Window::Current->Bounds.Width / 256);
        }
    }
    else
    {
        if (Window::Current)
        {
            scaleVec = DirectX::SimpleMath::Vector2(Window::Current->Bounds.Height / 192);
        }
    }

    m_batch->Draw(m_screen1SRV.Get(), XMFLOAT2(0, 0), nullptr, Colors::White, 0,
        scaleVec);

    if (horizontal == 2)
    {
        m_batch->Draw(m_screen2SRV.Get(), XMFLOAT2(256 * scaleVec.x, 0), nullptr, Colors::White, 0,
            scaleVec);
    }
    else
    {
        m_batch->Draw(m_screen2SRV.Get(), XMFLOAT2(0, 192 * scaleVec.y), nullptr, Colors::White, 0,
            scaleVec);
    }

    std::wstring fpsStr = std::to_wstring(m_emulator->GetFps());
    XMVECTOR origin = m_font->MeasureString(fpsStr.data()) / 2.f;

    m_font->DrawString(m_batch.get(), fpsStr.data(), DirectX::SimpleMath::Vector2{ 512, 256 },
        Colors::White, 0.f, origin);

    m_batch->End();
}

void EmulatorRenderer::CreateDeviceDependentResources()
{
    D3D11_TEXTURE2D_DESC screentexDes;
    screentexDes.Width = 256;
    screentexDes.Height = 192;
    screentexDes.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    screentexDes.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    screentexDes.MipLevels = 1;
    screentexDes.Usage = D3D11_USAGE_DYNAMIC;
    screentexDes.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    screentexDes.ArraySize = 1;
    screentexDes.MiscFlags = 0;
    screentexDes.SampleDesc.Count = 1;
    screentexDes.SampleDesc.Quality = 0;

    auto device = m_deviceResources->GetD3DDevice();

    device->CreateTexture2D(&screentexDes, NULL, &m_screen1Texture);
    device->CreateTexture2D(&screentexDes, NULL, &m_screen2Texture);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipLevels = 1;
    desc.Texture2D.MostDetailedMip = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

    device->CreateShaderResourceView(m_screen1Texture.Get(), &desc, &m_screen1SRV);
    device->CreateShaderResourceView(m_screen2Texture.Get(), &desc, &m_screen2SRV);

    auto context = m_deviceResources->GetD3DDeviceContext();

    m_batch = std::make_unique<DirectX::SpriteBatch>(context);
    m_font = std::make_unique<DirectX::SpriteFont>(device, L"Assets\\Default.spritefont");

    m_loadingComplete = true;
}

void EmulatorRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;

    m_screen1SRV.Reset();
    m_screen2SRV.Reset();
    m_screen1Texture.Reset();
    m_screen2Texture.Reset();

    m_batch.reset();
}