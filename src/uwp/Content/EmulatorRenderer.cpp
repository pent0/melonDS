#include "pch.h"
#include "EmulatorRenderer.h"

#include "..\Common\DirectXHelper.h"

#include <GPU.h>
#include <Config.h>

#include <SimpleMath.h>

#define CROSS_TEXTURE_FILE_NAME						L"Assets/Direct3D/pad_cross.dds"
#define START_TEXTURE_FILE_NAME						L"Assets/Direct3D/pad_start.dds"
#define SELECT_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_select.dds"
#define TURBO_TEXTURE_FILE_NAME						L"Assets/Direct3D/pad_turbo_button.dds"
#define COMBO_TEXTURE_FILE_NAME						L"Assets/Direct3D/pad_combo_button.dds"
#define A_TEXTURE_FILE_NAME							L"Assets/Direct3D/pad_a_button.dds"
#define B_TEXTURE_FILE_NAME							L"Assets/Direct3D/pad_b_button.dds"
#define L_TEXTURE_FILE_NAME							L"Assets/Direct3D/pad_l_button.dds"
#define R_TEXTURE_FILE_NAME							L"Assets/Direct3D/pad_r_button.dds"
#define STICK_TEXTURE_FILE_NAME						L"Assets/Direct3D/thumbstick.dds"
#define STICK_CENTER_TEXTURE_FILE_NAME				L"Assets/Direct3D/thumbstickcenter.dds"

#define CROSS_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_cross_color.dds"
#define START_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_start_color.dds"
#define SELECT_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_select_color.dds"
#define TURBO_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_turbo_button_color.dds"
#define COMBO_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_combo_button_color.dds"
#define A_COLOR_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_a_button_color.dds"
#define B_COLOR_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_b_button_color.dds"
#define L_COLOR_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_l_button_color.dds"
#define R_COLOR_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_r_button_color.dds"
#define STICK_COLOR_TEXTURE_FILE_NAME				L"Assets/Direct3D/thumbstick_color.dds"

#define CROSS_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_cross_gbasp.dds"
#define START_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_start_gbasp.dds"
#define SELECT_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_select_gbasp.dds"
#define TURBO_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_turbo_button_gbasp.dds"
#define COMBO_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/pad_combo_button_gbasp.dds"
#define A_GBASP_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_a_button_gbasp.dds"
#define B_GBASP_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_b_button_gbasp.dds"
#define L_GBASP_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_l_button_gbasp.dds"
#define R_GBASP_TEXTURE_FILE_NAME					L"Assets/Direct3D/pad_r_button_gbasp.dds"
#define STICK_GBASP_TEXTURE_FILE_NAME				L"Assets/Direct3D/thumbstick_gbasp.dds"

#define DIVIDER_TEXTURE_FILE_NAME					L"Assets/Direct3D/divider.dds"

using namespace uwp;

using namespace DirectX;
using namespace Windows::Foundation;

using namespace Windows::UI::Xaml;
using namespace Windows::Graphics::Display;

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

void EmulatorRenderer::OnOrientationChanged(DisplayInformation^ sender, Platform::Object^ args)
{
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
        m_hostFPS = timer.GetFramesPerSecond();

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

    m_batch->SetRotation(m_deviceResources->ComputeDisplayRotation());
    m_batch->Begin();

    bool horizontal = false;
    Vector2 scaleV = m_emulator->GetScale();
    
    m_batch->Draw(m_screen1SRV.Get(), m_emulator->GetScreen1Pos(), nullptr, Colors::White, 0, XMFLOAT2(0, 0),
        scaleV.x);

    m_batch->Draw(m_screen2SRV.Get(), m_emulator->GetScreen2Pos(), nullptr, Colors::White, 0,
        XMFLOAT2(0, 0), scaleV.x);

    std::wstring fpsStr = std::to_wstring(m_emulator->GetFps()).substr(0, 4) + L"(" + 
        std::to_wstring(m_hostFPS).substr(0, 4) + L")";

    Vector2 fontSize = m_font->MeasureString(fpsStr.data());
    XMVECTOR origin = fontSize / 2.0f;

    Vector2 textScale = Vector2{ (192 * scaleV.x) / 18 } / fontSize.y;
    
    m_font->DrawString(m_batch.get(), fpsStr.data(), m_emulator->GetScreen1Pos(),
        Colors::White, 0.f, Vector2{ 0, 0 }, textScale);

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

    m_screen1SRV->Release();
    m_screen2SRV->Release();
    m_screen1Texture->Release();
    m_screen2Texture->Release();

    m_screen1SRV.Reset();
    m_screen2SRV.Reset();
    m_screen1Texture.Reset();
    m_screen2Texture.Reset();

    m_batch.reset();
    m_font.reset();
}