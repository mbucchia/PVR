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

#ifndef USE_PVR
static const auto pvrEye_Count = 2;
#define CHECK_XRCMD(cmd) assert(XR_SUCCEEDED(cmd))
#endif

class PvrTextureSwapChains
{
public:
	PvrTextureSwapChains()
		:m_texSwapChain(nullptr)
		, m_session(nullptr)
	{

	}
	~PvrTextureSwapChains() {
#ifdef USE_PVR
		if (m_texSwapChain) {
			pvr_destroyTextureSwapChain(m_session, m_texSwapChain);
		}
#else
		if (m_texSwapChain) {
			CHECK_XRCMD(xrDestroySwapchain(m_texSwapChain));
		}
#endif
	}

#ifdef USE_PVR
	bool init(pvrTextureFormat format, int width, int height, pvrSessionHandle session, ID3D11Device* device, DXGI_FORMAT rtvFormat)
#else
	bool init(DXGI_FORMAT format, int width, int height, XrSession session, ID3D11Device* device, DXGI_FORMAT rtvFormat)
#endif
	{
		m_session = session;
		m_width = width;
		m_height = height;
#ifdef USE_PVR
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
#else
		XrSwapchainCreateInfo swapchainInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
		swapchainInfo.arraySize = 1;
		swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swapchainInfo.format = (int64_t)format;
		swapchainInfo.width = width;
		swapchainInfo.height = height;
		swapchainInfo.mipCount = 1;
		swapchainInfo.sampleCount = 1;
		swapchainInfo.faceCount = 1;
		CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainInfo, &m_texSwapChain));

		uint32_t imageCount;
		xrEnumerateSwapchainImages(m_texSwapChain, 0, &imageCount, nullptr);
		m_length = imageCount;
		std::vector<XrSwapchainImageD3D11KHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
		CHECK_XRCMD(xrEnumerateSwapchainImages(m_texSwapChain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data()));
#endif
		for (int i = 0; i < m_length; i++)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
#ifdef USE_PVR
			ret = pvr_getTextureSwapChainBufferDX(session, m_texSwapChain, i, IID_ID3D11Texture2D, (void**)tex.GetAddressOf());
			if (ret != pvr_success) {
				pvr_logMessage(session->envh, pvrLogLevel::pvrErr, "pvr_getTextureSwapChainBufferDX failed");
				return false;
			}
#else
			tex = images[i].texture;
#endif
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
#ifndef USE_PVR
		CHECK_XRCMD(xrAcquireSwapchainImage(m_texSwapChain, nullptr, &m_lastAcquiredIndex));
#endif
		return true;
	}
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRenderTargetView() {
		int idx = 0;
#ifdef USE_PVR
		auto ret = pvr_getTextureSwapChainCurrentIndex(m_session, m_texSwapChain, &idx);
		if (ret != pvr_success) {
			return nullptr;
		}
#else
		XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		waitInfo.timeout = 1000000000;
		CHECK_XRCMD(xrWaitSwapchainImage(m_texSwapChain, &waitInfo));
		idx = m_lastAcquiredIndex;
#endif
		return m_RTVs[idx];
	}
	Microsoft::WRL::ComPtr<ID3D11Texture2D> currentTexture() {
		int idx = 0;
#ifdef USE_PVR
		auto ret = pvr_getTextureSwapChainCurrentIndex(m_session, m_texSwapChain, &idx);
		if (ret != pvr_success) {
			return nullptr;
		}
#else
		XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		waitInfo.timeout = 1000000000;
		CHECK_XRCMD(xrWaitSwapchainImage(m_texSwapChain, &waitInfo));
		idx = m_lastAcquiredIndex;
#endif
		return m_Texs[idx];
	}
	bool commit() {
#ifdef USE_PVR
		return (pvr_success == pvr_commitTextureSwapChain(m_session, m_texSwapChain));
#else
		CHECK_XRCMD(xrReleaseSwapchainImage(m_texSwapChain, nullptr));
		CHECK_XRCMD(xrAcquireSwapchainImage(m_texSwapChain, nullptr, &m_lastAcquiredIndex));
		return true;
#endif
	}
	int getWidth() {
		return m_width;
	}
	int getHeight() {
		return m_height;
	}
#ifdef USE_PVR
	pvrTextureSwapChain swapChain() {
#else
	XrSwapchain swapChain() {
#endif
		return m_texSwapChain;
	}
#ifdef USE_PVR
	static std::shared_ptr<PvrTextureSwapChains> create(pvrTextureFormat format, int width, int height, pvrSessionHandle session, ID3D11Device* device, DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN) {
#else
	static std::shared_ptr<PvrTextureSwapChains> create(DXGI_FORMAT format, int width, int height, XrSession session, ID3D11Device * device, DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN) {
#endif
		auto ret = std::make_shared<PvrTextureSwapChains>();
		if (ret->init(format, width, height, session, device, rtvFormat)) {
			return ret;
		}
		return nullptr;
	}
private:
#ifdef USE_PVR
	pvrTextureSwapChain		m_texSwapChain;
	pvrSessionHandle		m_session;
#else
	XrSwapchain				m_texSwapChain;
	XrSession				m_session;
	uint32_t				m_lastAcquiredIndex;
#endif
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

#ifdef USE_PVR
	pvrEnvHandle							m_pvrEnv;
	pvrSessionHandle						m_pvrSession;
#else
	XrInstance								m_xrInstance;
	XrSystemId								m_xrSystemId;
	XrSession								m_xrSession;
	XrSpace									m_xrLocalSpace;
	XrSpace									m_xrViewSpace;
#endif
	bool									m_hmdReady;
	bool									m_hasVrFocus;

	bool									m_showQuad = false;
	std::shared_ptr<PvrTextureSwapChains>   m_quadTex;
	DirectX::SimpleMath::Vector2			m_quadCenter;
	RECT									m_quadScreenRect;
#ifdef USE_PVR
	pvrVector2f								m_quadSize;
#else
	XrExtent2Df								m_quadSize;
#endif

	std::shared_ptr<PvrTextureSwapChains>   m_eyeTexs[pvrEye_Count];
#ifdef USE_PVR
	pvrMirrorTexture						m_mirrorTexSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_mirrorTex;
#endif

	Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_depthStencil[pvrEye_Count];
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_d3dDepthStencilView[pvrEye_Count];

	std::unique_ptr<DirectX::Keyboard> m_keyboard;
	DirectX::Keyboard::KeyboardStateTracker m_keyStateTracker;
	std::unique_ptr<DirectX::Mouse> m_mouse;

	uint32_t m_lastHandButtons[2];
};