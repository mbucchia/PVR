//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include <iomanip>
#include <sstream>

extern void ExitGame();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

static const XMVECTORF32 START_POSITION = { 0.f, -1.5f, 0.f, 0.f };
static const XMVECTORF32 ROOM_BOUNDS = { 8.f, 6.f, 12.f, 0.f };

Game::Game()
{
	m_lastHandButtons[0] = 0;
	m_lastHandButtons[1] = 0;
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, 
		DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0, DX::DeviceResources::c_AllowTearing
		);
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
	m_quadTex.reset();
	m_eyeTexs[0].reset();
	m_eyeTexs[1].reset();
	if (m_mirrorTexSwapChain) {
		pvr_destroyMirrorTexture(m_pvrSession, m_mirrorTexSwapChain);
	}
	if (m_pvrSession) {
		pvr_destroySession(m_pvrSession);
	}
	if (m_pvrEnv) {
		pvr_shutdown(m_pvrEnv);
	}
}

#define ExitIfNot(exp) \
if(!(exp)) {\
MessageBoxA(NULL, "Check ("#exp##") failed.", "", MB_OK);\
ExitGame();\
return;\
}

#define ExitIfFailed(exp) ExitIfNot(pvr_success == (exp))

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
	ExitIfFailed(pvr_initialise(&m_pvrEnv));
	ExitIfFailed(pvr_createSession(m_pvrEnv, &m_pvrSession))
	pvrHmdStatus hmdStatus;
	ExitIfFailed(pvr_getHmdStatus(m_pvrSession, &hmdStatus))
	ExitIfNot(hmdStatus.ServiceReady);
	ExitIfNot(hmdStatus.HmdPresent);
	ExitIfNot(!hmdStatus.DisplayLost);

	pvr_setTrackingOriginType(m_pvrSession, pvrTrackingOrigin::pvrTrackingOrigin_FloorLevel);

	pvrDisplayInfo dispInfo;
	ExitIfFailed(pvr_getEyeDisplayInfo(m_pvrSession, pvrEye_Left, &dispInfo));

    m_deviceResources->SetWindow(window, width, height);
	m_deviceResources->SetAdapterId(*(LUID*)&dispInfo.luid);
    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	m_keyboard = std::make_unique<Keyboard>();
	m_mouse = std::make_unique<Mouse>();
	m_mouse->SetWindow(window);

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

	auto kb = m_keyboard->GetState();
	m_keyStateTracker.Update(kb);
	if (kb.Escape) {
		ExitGame();
		return;
	}

	if (m_keyStateTracker.IsKeyPressed(Keyboard::Keys::Space)) {
		m_showQuad = !m_showQuad;
	}

	pvrHmdStatus hmdStatus;
	auto ret = pvr_getHmdStatus(m_pvrSession, &hmdStatus);
	if (hmdStatus.ShouldQuit) {
		ExitGame();
		return;
	}
	m_hmdReady = (ret == pvr_success
		&& hmdStatus.HmdPresent
		&& !hmdStatus.DisplayLost
		&& hmdStatus.ServiceReady);
	m_hasVrFocus = (m_hmdReady && hmdStatus.IsVisible);
	if (m_hasVrFocus) {
		pvrInputState inputState;
		ret = pvr_getInputState(m_pvrSession, &inputState);
		if (ret == pvr_success) {
			uint32_t releasedButtons[2];
			releasedButtons[0] = ~(inputState.HandButtons[0]) & m_lastHandButtons[0];
			releasedButtons[1] = ~(inputState.HandButtons[1]) & m_lastHandButtons[1];
			if (releasedButtons[0] & pvrButton_ApplicationMenu
				|| releasedButtons[1] & pvrButton_ApplicationMenu)
			{
				m_showQuad = !m_showQuad;
			}
			m_lastHandButtons[0] = inputState.HandButtons[0];
			m_lastHandButtons[1] = inputState.HandButtons[1];
		}
	}
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

	if (!m_hmdReady) {
		return;
	}
	if (!m_hasVrFocus) {
		return;
	}

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Add your rendering code here.
    context;

	pvrEyeRenderInfo eyeRenderInfo[pvrEye_Count];
	pvr_getEyeRenderInfo(m_pvrSession, pvrEye_Left, &eyeRenderInfo[pvrEye_Left]);
	pvr_getEyeRenderInfo(m_pvrSession, pvrEye_Right, &eyeRenderInfo[pvrEye_Right]);
	pvrPosef hmdToEyePose[2];
	hmdToEyePose[0] = eyeRenderInfo[0].HmdToEyePose;
	hmdToEyePose[1] = eyeRenderInfo[1].HmdToEyePose;

        pvr_waitToBeginFrame(m_pvrSession, 0);
	pvr_beginFrame(m_pvrSession, 0);

	auto display_time = pvr_getPredictedDisplayTime(m_pvrSession, 0);
	pvrTrackingState trackingState;
	auto ret = pvr_getTrackingState(m_pvrSession, display_time, &trackingState);
	if (ret != pvr_success) {
		pvr_logMessage(m_pvrEnv, pvrErr, "pvr_getTrackingState failed");
		return;
	}
	if (trackingState.HeadPose.StatusFlags == 0) {
		pvr_logMessage(m_pvrEnv, pvrWarn, "hmd not tracking.");
		return;
	}
	
	pvrPosef eyePoses[2];
	pvr_calcEyePoses(m_pvrEnv, trackingState.HeadPose.ThePose, hmdToEyePose, eyePoses);

	pvrLayerEyeFov eyeLayer;
	eyeLayer.Header.Type = pvrLayerType_Disabled;
	if (m_eyeTexs[0] && m_eyeTexs[1]) {

		for (int i = 0; i < pvrEye_Count; i++)
		{
			auto renderTarget = m_eyeTexs[i]->currentRenderTargetView().Get();
			context->ClearRenderTargetView(renderTarget, Colors::Black);
			context->ClearDepthStencilView(m_d3dDepthStencilView[i].Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			context->OMSetRenderTargets(1, &renderTarget, m_d3dDepthStencilView[i].Get());
			D3D11_VIEWPORT viewport;
			viewport.Height = static_cast<float>(m_eyeTexs[i]->getHeight());
			viewport.Width = static_cast<float>(m_eyeTexs[i]->getWidth());
			viewport.MaxDepth = 1.0f;
			viewport.MinDepth = 0.0f;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			context->RSSetViewports(1, &viewport);

			context->OMSetBlendState(m_states->AlphaBlend(), nullptr, 0xFFFFFFFF);
			context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
			context->RSSetState(m_states->CullClockwise());

			
			Quaternion eyeRotation = { eyePoses[i].Orientation.x, eyePoses[i].Orientation.y,eyePoses[i].Orientation.z,eyePoses[i].Orientation.w};
			Matrix eyeRotationMat = Matrix::CreateFromQuaternion(eyeRotation);
			Vector3 up = { 0,1,0 };
			Vector3 forward = { 0,0,-1 };
			Vector3::Transform(up, eyeRotation, up);
			Vector3::Transform(forward, eyeRotation, forward);
			Vector3 eyePos = { eyePoses[i].Position.x,eyePoses[i].Position.y,eyePoses[i].Position.z };
			m_view = Matrix::CreateLookAt(eyePos,
				eyePos+ forward, up);
			pvrMatrix4f projMat;
			pvrMatrix4f_Projection(m_pvrEnv, eyeRenderInfo[i].Fov, 0.01f, 1000.0f, pvrTrue, &projMat);
			m_proj = Matrix(projMat.M[0][0], projMat.M[1][0], projMat.M[2][0], projMat.M[3][0],
				projMat.M[0][1], projMat.M[1][1], projMat.M[2][1], projMat.M[3][1],
				projMat.M[0][2], projMat.M[1][2], projMat.M[2][2], projMat.M[3][2],
				projMat.M[0][3], projMat.M[1][3], projMat.M[2][3], projMat.M[3][3]
			);

			Vector3 roomSize = ROOM_BOUNDS.v;
			m_room->Draw(Matrix::CreateTranslation({0,roomSize.y/2,0}), m_view, m_proj, Colors::White, m_roomTex.Get(), false, nullptr);

			m_shapeEffect->SetView(m_view);
			m_shapeEffect->SetProjection(m_proj);
			m_world = Matrix::CreateRotationY((float)m_timer.GetTotalSeconds()*0.5f)*Matrix::CreateTranslation(-0.0f, 1.2f, -1.50f);
			m_shapeEffect->SetWorld(m_world);
			m_shape->Draw(m_shapeEffect.get(), m_shapeInputLayout.Get());

			for (int hand = 0; hand < 2; hand++)
			{
				if (trackingState.HandPoses[hand].StatusFlags != 0) {
					auto handPose = trackingState.HandPoses[hand].ThePose;
					m_world = Matrix::CreateFromQuaternion(Quaternion(handPose.Orientation.x, handPose.Orientation.y, handPose.Orientation.z, handPose.Orientation.w))
						*Matrix::CreateScale(0.1f)
						*Matrix::CreateTranslation(handPose.Position.x, handPose.Position.y, handPose.Position.z);
					m_shapeHands[hand]->Draw(m_world, m_view, m_proj, Colors::White);
				}
			}

			m_eyeTexs[i]->commit();
			eyeLayer.ColorTexture[i] = m_eyeTexs[i]->swapChain();
			eyeLayer.Fov[i] = eyeRenderInfo[i].Fov;
			eyeLayer.RenderPose[i] = eyePoses[i];
			eyeLayer.Viewport[i] = { 0, 0,m_eyeTexs[i]->getWidth(), m_eyeTexs[i]->getHeight() };
		}
		
		eyeLayer.Header.Type = pvrLayerType_EyeFov;
		eyeLayer.Header.Flags = 0;
		eyeLayer.SensorSampleTime = trackingState.HeadPose.TimeInSeconds;
	}


	pvrLayerQuad quadLayer;
	quadLayer.Header.Type = pvrLayerType_Disabled;
	if (m_showQuad && m_quadTex) {
		auto renderTarget = m_quadTex->currentRenderTargetView().Get();
		context->ClearRenderTargetView(renderTarget, Colors::DarkGreen);
		context->OMSetRenderTargets(1, &renderTarget, nullptr);
		D3D11_VIEWPORT viewport;
		viewport.Height = static_cast<float>(m_quadTex->getHeight());
		viewport.Width = static_cast<float>(m_quadTex->getWidth());
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = 0.0f;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		context->RSSetViewports(1, &viewport);
		m_spriteBatch->Begin(SpriteSortMode_Deferred, m_states->NonPremultiplied());

		auto fps = 1.0f / (float)m_timer.GetElapsedSeconds();
		std::wostringstream os;
		os << "FPS:" << std::setprecision(4) << fps;
		Vector2 origin = m_font->MeasureString(os.str().c_str())/2;

		m_font->DrawString(m_spriteBatch.get(), os.str().c_str(),
		{ (float)m_quadScreenRect.right/2,(float)m_quadScreenRect.bottom/2}, Colors::White, 0.f, origin, 0.5f);

		m_spriteBatch->End();

		m_quadTex->commit();
		quadLayer.Header.Type = pvrLayerType_Quad;
		quadLayer.Header.Flags = pvrLayerFlag_HeadLocked;
		quadLayer.ColorTexture = m_quadTex->swapChain();
		quadLayer.QuadPoseCenter.Position = { 0.0,0.0,-3.0f };
		quadLayer.QuadPoseCenter.Orientation = { 0,0,0,1.0f };
		quadLayer.QuadSize = m_quadSize;
		quadLayer.Viewport = { 0, 0,m_quadTex->getWidth(), m_quadTex->getHeight() };
	}
    m_deviceResources->PIXEndEvent();

	pvrLayerHeader* layers[] = { &eyeLayer.Header,&quadLayer.Header };
	ret = pvr_endFrame(m_pvrSession, 0, layers, _countof(layers));
	if (ret != pvr_success) {
		pvr_logMessage(m_pvrEnv, pvrLogLevel::pvrErr, "pvr_submitFrame failed.");
	}

	if (m_mirrorTex) {
		context->CopyResource(m_deviceResources->GetRenderTarget(), m_mirrorTex.Get());
	}

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // TODO: Initialize device dependent objects here (independent of window size).
    device;

	m_quadTex = PvrTextureSwapChains::create(pvrTextureFormat::PVR_FORMAT_B8G8R8A8_UNORM_SRGB, 300, 200, m_pvrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);
	m_quadCenter.x = m_quadTex->getWidth() / 2.f;
	m_quadCenter.y = m_quadTex->getHeight() / 2.f;
	m_quadScreenRect.left = 0;
	m_quadScreenRect.top = 0;
	m_quadScreenRect.right = m_quadTex->getWidth();
	m_quadScreenRect.bottom = m_quadTex->getHeight();
	m_quadSize.y = 1.0f;
	m_quadSize.x = m_quadSize.y*(float)m_quadTex->getWidth() / m_quadTex->getHeight();
	
	pvrEyeRenderInfo eyeRenderInfo[pvrEye_Count];
	for (int i = 0; i < pvrEye_Count; i++)
	{
		auto eye = (i == 0 ? pvrEye_Left : pvrEye_Right);
		ExitIfFailed(pvr_getEyeRenderInfo(m_pvrSession, eye, &eyeRenderInfo[eye]));
		pvrSizei texSize;
		auto ret = pvr_getFovTextureSize(m_pvrSession, eye, eyeRenderInfo[eye].Fov, 1.0f, &texSize);
		if (ret != pvr_success) {
			pvr_logMessage(m_pvrEnv, pvrErr, "pvr_getFovTextureSize failed.");
			return;
		}
		m_eyeTexs[i] = PvrTextureSwapChains::create(pvrTextureFormat::PVR_FORMAT_B8G8R8A8_UNORM_SRGB, texSize.w, texSize.h, m_pvrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);

		// Create a depth stencil view for use with 3D rendering if needed.
		CD3D11_TEXTURE2D_DESC depthStencilDesc(
			DXGI_FORMAT_D32_FLOAT,
			texSize.w,
			texSize.h,
			1, // This depth stencil view has only one texture.
			1, // Use a single mipmap level.
			D3D11_BIND_DEPTH_STENCIL
		);

		DX::ThrowIfFailed(device->CreateTexture2D(
			&depthStencilDesc,
			nullptr,
			m_depthStencil[i].ReleaseAndGetAddressOf()
		));

		CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
		DX::ThrowIfFailed(device->CreateDepthStencilView(
			m_depthStencil[i].Get(),
			&depthStencilViewDesc,
			m_d3dDepthStencilView[i].ReleaseAndGetAddressOf()
		));
	}
	
	m_spriteBatch = std::make_unique<SpriteBatch>(m_deviceResources->GetD3DDeviceContext());
	m_states = std::make_unique<CommonStates>(device);

	m_font = std::make_unique<SpriteFont>(device, L"myFont.spritefont");

	m_world = Matrix::Identity;

	m_shapeEffect = std::make_unique<BasicEffect>(device);
	m_shapeEffect->SetTextureEnabled(true);
	m_shapeEffect->SetPerPixelLighting(true);
	m_shapeEffect->SetLightingEnabled(true);
	m_shapeEffect->SetLightEnabled(0, true);
	m_shapeEffect->SetLightDiffuseColor(0, Colors::White);
	m_shapeEffect->SetLightDirection(0, -Vector3::UnitZ);
	m_shape = GeometricPrimitive::CreateSphere(m_deviceResources->GetD3DDeviceContext());
	m_shape->CreateInputLayout(m_shapeEffect.get(),
		m_shapeInputLayout.ReleaseAndGetAddressOf());

	DX::ThrowIfFailed(
		CreateWICTextureFromFile(device, L"earth.bmp", nullptr,
			m_earthTexture.ReleaseAndGetAddressOf()));

	m_shapeEffect->SetTexture(m_earthTexture.Get());

	m_shapeHands[0] = GeometricPrimitive::CreateCone(m_deviceResources->GetD3DDeviceContext());
	m_shapeHands[1] = GeometricPrimitive::CreateCylinder(m_deviceResources->GetD3DDeviceContext());

	m_room = GeometricPrimitive::CreateBox(m_deviceResources->GetD3DDeviceContext(),
		XMFLOAT3(ROOM_BOUNDS[0], ROOM_BOUNDS[1], ROOM_BOUNDS[2]),
		false, true);

	DX::ThrowIfFailed(
		CreateDDSTextureFromFile(device, L"roomtexture.dds",
			nullptr, m_roomTex.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.
	if (m_mirrorTexSwapChain) {
		pvr_destroyMirrorTexture(m_pvrSession, m_mirrorTexSwapChain);
		m_mirrorTexSwapChain = nullptr;
	}
	pvrMirrorTextureDesc mirrorDesc;
	mirrorDesc.Format = pvrTextureFormat::PVR_FORMAT_B8G8R8A8_UNORM_SRGB;
	auto outputSize = m_deviceResources->GetOutputSize();
	mirrorDesc.Width = outputSize.right - outputSize.left;
	mirrorDesc.Height = outputSize.bottom - outputSize.top;
	mirrorDesc.SampleCount = 1;
	mirrorDesc.MiscFlags = pvrTextureMisc_DX_Typeless;
	pvr_createMirrorTextureDX(m_pvrSession, m_deviceResources->GetD3DDevice(), &mirrorDesc, &m_mirrorTexSwapChain);
	if (m_mirrorTexSwapChain) {
		pvr_getMirrorTextureBufferDX(m_pvrSession, m_mirrorTexSwapChain, IID_ID3D11Texture2D, (void**)m_mirrorTex.ReleaseAndGetAddressOf());
	}

}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
	m_quadTex.reset();
	m_eyeTexs[0].reset();
	m_eyeTexs[1].reset();
	m_spriteBatch.reset();
	m_states.reset();
	m_font.reset();
	m_shape.reset();
	m_shapeHands[0].reset();
	m_shapeHands[1].reset();
	m_earthTexture.Reset();
	m_mirrorTex.Reset();
	if (m_mirrorTexSwapChain) {
		pvr_destroyMirrorTexture(m_pvrSession, m_mirrorTexSwapChain);
	}
	m_room.reset();
	m_roomTex.Reset();

}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
