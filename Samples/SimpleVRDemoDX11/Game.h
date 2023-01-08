//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include <vector>

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

class PvrTextureSwapChains
{
public:
	PvrTextureSwapChains()
		:m_texSwapChain(nullptr)
		, m_session(nullptr)
	{

	}
	~PvrTextureSwapChains() {
		if (m_texSwapChain) {
			pvr_destroyTextureSwapChain(m_session, m_texSwapChain);
		}
	}

	bool init(pvrTextureFormat format, int width, int height, pvrSessionHandle session, ID3D11Device* device, DXGI_FORMAT rtvFormat)
	{
		m_session = session;
		m_width = width;
		m_height = height;
		pvrTextureSwapChainDesc desc;
		desc.ArraySize = 1;
		desc.BindFlags = pvrTextureBind_DX_RenderTarget;
		desc.Format = format;
		desc.Height = height;
		desc.Width = width;
		desc.MipLevels = 1;
		if (rtvFormat != DXGI_FORMAT_UNKNOWN) {
			desc.MiscFlags = pvrTextureMisc_DX_Typeless;
		}
		else {
			desc.MiscFlags = 0;
		}
		desc.SampleCount = 1;
		desc.StaticImage = pvrFalse;
		desc.Type = pvrTextureType::pvrTexture_2D;
		auto ret = pvr_createTextureSwapChainDX(session, device, &desc, &m_texSwapChain);
		if (ret != pvr_success) {
			pvr_logMessage(session->envh, pvrLogLevel::pvrErr, "pvr_createTextureSwapChainDX failed");
			return false;
		}
		ret = pvr_getTextureSwapChainLength(session, m_texSwapChain, &m_length);
		if (ret != pvr_success) {
			pvr_logMessage(session->envh, pvrLogLevel::pvrErr, "pvr_getTextureSwapChainLength failed");
			return false;
		}
		for (int i = 0; i < m_length; i++)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
			ret = pvr_getTextureSwapChainBufferDX(session, m_texSwapChain, i, IID_ID3D11Texture2D, (void**)tex.GetAddressOf());
			if (ret != pvr_success) {
				pvr_logMessage(session->envh, pvrLogLevel::pvrErr, "pvr_getTextureSwapChainBufferDX failed");
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
			D3D11_RENDER_TARGET_VIEW_DESC desc;
			desc.Format = rtvFormat;
			desc.Texture2D.MipSlice = 0;
			desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			auto hr = device->CreateRenderTargetView(tex.Get(), (rtvFormat == DXGI_FORMAT_UNKNOWN) ? nullptr :&desc, rtv.GetAddressOf());
			if (FAILED(hr)) {
				return false;
			}
			m_RTVs.push_back(rtv);
			m_Texs.push_back(tex);
		}
		return true;
	}
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRenderTargetView() {
		int idx = 0;
		auto ret = pvr_getTextureSwapChainCurrentIndex(m_session, m_texSwapChain, &idx);
		if (ret != pvr_success) {
			return nullptr;
		}
		return m_RTVs[idx];
	}
	Microsoft::WRL::ComPtr<ID3D11Texture2D> currentTexture() {
		int idx = 0;
		auto ret = pvr_getTextureSwapChainCurrentIndex(m_session, m_texSwapChain, &idx);
		if (ret != pvr_success) {
			return nullptr;
		}
		return m_Texs[idx];
	}
	bool commit() {
		return (pvr_success == pvr_commitTextureSwapChain(m_session, m_texSwapChain));
	}
	int getWidth() {
		return m_width;
	}
	int getHeight() {
		return m_height;
	}
	pvrTextureSwapChain swapChain() {
		return m_texSwapChain;
	}
	static std::shared_ptr<PvrTextureSwapChains> create(pvrTextureFormat format, int width, int height, pvrSessionHandle session, ID3D11Device* device, DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN) {
		auto ret = std::make_shared<PvrTextureSwapChains>();
		if (ret->init(format, width, height, session, device, rtvFormat)) {
			return ret;
		}
		return nullptr;
	}
private:
	pvrTextureSwapChain		m_texSwapChain;
	pvrSessionHandle		m_session;
	int						m_length;
	int						m_width;
	int						m_height;
	std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> m_RTVs;
	std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> m_Texs;
};


// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game : public DX::IDeviceNotify
{
public:

    Game();
	virtual ~Game();

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

	std::unique_ptr<DirectX::SpriteBatch>	m_spriteBatch;

	std::unique_ptr<DirectX::CommonStates>	m_states;

	std::unique_ptr<DirectX::SpriteFont>	m_font;

	DirectX::SimpleMath::Matrix				m_world;
	DirectX::SimpleMath::Matrix				m_view;
	DirectX::SimpleMath::Matrix				m_proj;

	std::unique_ptr<DirectX::GeometricPrimitive> m_shape;
	std::unique_ptr<DirectX::BasicEffect>	m_shapeEffect;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_shapeInputLayout;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_earthTexture;

	std::unique_ptr<DirectX::GeometricPrimitive> m_shapeHands[2];

	std::unique_ptr<DirectX::GeometricPrimitive> m_room;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_roomTex;

	pvrEnvHandle							m_pvrEnv;
	pvrSessionHandle						m_pvrSession;
	bool									m_hmdReady;
	bool									m_hasVrFocus;

	bool									m_showQuad = false;
	std::shared_ptr<PvrTextureSwapChains>   m_quadTex;
	DirectX::SimpleMath::Vector2			m_quadCenter;
	RECT									m_quadScreenRect;
	pvrVector2f								m_quadSize;

	std::shared_ptr<PvrTextureSwapChains>   m_eyeTexs[pvrEye_Count];
	pvrMirrorTexture						m_mirrorTexSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_mirrorTex;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil[pvrEye_Count];
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView[pvrEye_Count];

	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	DirectX::Keyboard::KeyboardStateTracker m_keyStateTracker;
	std::unique_ptr<DirectX::Mouse> m_mouse;

	uint32_t m_lastHandButtons[2];
};