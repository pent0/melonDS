#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"
#include "Emulator.h"

#include <SpriteBatch.h>
#include <SpriteFont.h>

using namespace Windows::Graphics::Display;

namespace uwp
{
	// This sample renderer instantiates a basic rendering pipeline.
	class EmulatorRenderer
	{
	public:
		EmulatorRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources,
            const std::shared_ptr<uwp::Emulator> &emulatorInst);

		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();
		void StartTracking();
		void TrackingUpdate(float positionX);
		void StopTracking();
		bool IsTracking() { return m_tracking; }

	private:
		void UpdateScreen();
        void OnOrientationChanged(DisplayInformation^ sender, Platform::Object^ args);

	private:
		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

        // Pointer to the emulator instance.
        std::shared_ptr<uwp::Emulator> m_emulator;

        Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_screen1Texture;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_screen2Texture;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_screen1SRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_screen2SRV;

        float m_hostFPS;

        // Sprite batch, used for drawing control buttons and the framebuffer
        // bitmap
        std::unique_ptr<DirectX::SpriteBatch> m_batch;
        std::unique_ptr<DirectX::SpriteFont> m_font;

		// System resources for cube geometry.
		ModelViewProjectionConstantBuffer	m_constantBufferData;
		uint32	m_indexCount;

		// Variables used with the rendering loop.
		bool	m_loadingComplete;
		float	m_degreesPerSecond;
		bool	m_tracking;
	};
}