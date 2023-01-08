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

#if 1
#include <chrono>

namespace {

	// Utility logging function.
	void InternalLog(const char* fmt, va_list va, bool logTelemetry = false) {
		const std::time_t now = std::time(nullptr);

		char buf[1024];
		size_t offset = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %z: ", std::localtime(&now));
		vsnprintf_s(buf + offset, sizeof(buf) - offset, _TRUNCATE, fmt, va);
		OutputDebugStringA(buf);
	}
} // namespace

void Log(const char* fmt, ...) {
	va_list va;
	va_start(va, fmt);
	InternalLog(fmt, va);
	va_end(va);
}

// A generic timer.
struct ITimer {
	virtual ~ITimer() = default;

	virtual void start() = 0;
	virtual void stop() = 0;

	virtual uint64_t query(bool reset = true) const = 0;
};

// A CPU synchronous timer.
class CpuTimer : public ITimer {
	using clock = std::chrono::high_resolution_clock;

public:
	void start() override {
		m_timeStart = clock::now();
	}

	void stop() override {
		m_duration = clock::now() - m_timeStart;
	}

	uint64_t query(bool reset = true) const override {
		const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_duration);
		if (reset)
			m_duration = clock::duration::zero();
		return duration.count();
	}

private:
	clock::time_point m_timeStart;
	mutable clock::duration m_duration{ 0 };
};
#endif

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
#ifdef USE_PVR
	if (m_mirrorTexSwapChain) {
		pvr_destroyMirrorTexture(m_pvrSession, m_mirrorTexSwapChain);
	}
	if (m_pvrSession) {
		pvr_destroySession(m_pvrSession);
	}
	if (m_pvrEnv) {
		pvr_shutdown(m_pvrEnv);
	}
#else
	CHECK_XRCMD(xrDestroySpace(m_xrLocalSpace));
	CHECK_XRCMD(xrDestroySpace(m_xrViewSpace));
	CHECK_XRCMD(xrDestroySession(m_xrSession));
	CHECK_XRCMD(xrDestroyInstance(m_xrInstance));
#endif
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
#ifdef USE_PVR
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
#else
	XrInstanceCreateInfo instanceInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
	const char* extensions[] = {XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
	instanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
	sprintf_s(instanceInfo.applicationInfo.applicationName, "PVR_Sample");
	instanceInfo.enabledExtensionNames = extensions;
	instanceInfo.enabledExtensionCount = 1;
	CHECK_XRCMD(xrCreateInstance(&instanceInfo, &m_xrInstance));

	XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	CHECK_XRCMD(xrGetSystem(m_xrInstance, &systemInfo, &m_xrSystemId));

	XrGraphicsRequirementsD3D11KHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
	PFN_xrGetD3D11GraphicsRequirementsKHR xrGetD3D11GraphicsRequirementsKHR;
	CHECK_XRCMD(xrGetInstanceProcAddr(m_xrInstance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&xrGetD3D11GraphicsRequirementsKHR));
	CHECK_XRCMD(xrGetD3D11GraphicsRequirementsKHR(m_xrInstance, m_xrSystemId, &graphicsRequirements));

	m_deviceResources->SetWindow(window, width, height);
	m_deviceResources->SetAdapterId(graphicsRequirements.adapterLuid);
	m_deviceResources->CreateDeviceResources();

	XrGraphicsBindingD3D11KHR graphicsBindings{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
	graphicsBindings.device = m_deviceResources->GetD3DDevice();
	XrSessionCreateInfo sessionInfo{ XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.systemId = m_xrSystemId;
	sessionInfo.next = &graphicsBindings;
	CHECK_XRCMD(xrCreateSession(m_xrInstance, &sessionInfo, &m_xrSession));

	XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
	beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	CHECK_XRCMD(xrBeginSession(m_xrSession, &beginInfo));

	XrReferenceSpaceCreateInfo spaceInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	spaceInfo.poseInReferenceSpace.position = { 0,0,0 };
	spaceInfo.poseInReferenceSpace.orientation = { 0,0,0,1 };

	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	CHECK_XRCMD(xrCreateReferenceSpace(m_xrSession, &spaceInfo, &m_xrLocalSpace));
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	CHECK_XRCMD(xrCreateReferenceSpace(m_xrSession, &spaceInfo, &m_xrViewSpace));
#endif

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

#ifdef USE_PVR
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
#else
	m_hmdReady = m_hasVrFocus = true;
	m_lastHandButtons[0] = m_lastHandButtons[1] = 0;
#endif
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

#ifdef USE_PVR
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
#else
	XrFrameState frameState{ XR_TYPE_FRAME_STATE };
	CHECK_XRCMD(xrWaitFrame(m_xrSession, nullptr, &frameState));

	CHECK_XRCMD(xrBeginFrame(m_xrSession, nullptr));

	XrViewLocateInfo locateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
	locateInfo.displayTime = frameState.predictedDisplayTime;
	locateInfo.space = m_xrLocalSpace;
	locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

	XrViewState viewState{ XR_TYPE_VIEW_STATE };
	XrView views[2]{ {XR_TYPE_VIEW}, {XR_TYPE_VIEW} };
	uint32_t count;
	CHECK_XRCMD(xrLocateViews(m_xrSession, &locateInfo, &viewState, 2, &count, views));
#endif

#ifdef USE_PVR
	pvrLayerEyeFov eyeLayer;
	eyeLayer.Header.Type = pvrLayerType_Disabled;
#else
	XrCompositionLayerProjection eyeLayer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	XrCompositionLayerProjectionView eyeViews[2]{ {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW} };
	eyeLayer.views = eyeViews;
	eyeLayer.viewCount = 2;
#endif
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

#ifdef USE_PVR
			Quaternion eyeRotation = { eyePoses[i].Orientation.x, eyePoses[i].Orientation.y,eyePoses[i].Orientation.z,eyePoses[i].Orientation.w};
#else
			Quaternion eyeRotation = { views[i].pose.orientation.x, views[i].pose.orientation.y,views[i].pose.orientation.z,views[i].pose.orientation.w };
#endif
			Matrix eyeRotationMat = Matrix::CreateFromQuaternion(eyeRotation);
			Vector3 up = { 0,1,0 };
			Vector3 forward = { 0,0,-1 };
			Vector3::Transform(up, eyeRotation, up);
			Vector3::Transform(forward, eyeRotation, forward);
#ifdef USE_PVR
			Vector3 eyePos = { eyePoses[i].Position.x,eyePoses[i].Position.y,eyePoses[i].Position.z };
			m_view = Matrix::CreateLookAt(eyePos,
				eyePos + forward, up);
			pvrMatrix4f projMat;
			pvrMatrix4f_Projection(m_pvrEnv, eyeRenderInfo[i].Fov, 0.01f, 1000.0f, pvrTrue, &projMat);
			m_proj = Matrix(projMat.M[0][0], projMat.M[1][0], projMat.M[2][0], projMat.M[3][0],
				projMat.M[0][1], projMat.M[1][1], projMat.M[2][1], projMat.M[3][1],
				projMat.M[0][2], projMat.M[1][2], projMat.M[2][2], projMat.M[3][2],
				projMat.M[0][3], projMat.M[1][3], projMat.M[2][3], projMat.M[3][3]
			);
#else
			Vector3 eyePos = { views[i].pose.position.x,views[i].pose.position.y,views[i].pose.position.z };
			m_view = Matrix::CreateLookAt(eyePos,
				eyePos + forward, up); 
			{
				const float nearPlane = 0.01f;
				const float farPlane = 1000.0f;
				float l = tan(views[i].fov.angleLeft) * nearPlane;
				float r = tan(views[i].fov.angleRight) * nearPlane;
				float b = tan(views[i].fov.angleDown) * nearPlane;
				float t = tan(views[i].fov.angleUp) * nearPlane;
				m_proj = Matrix::CreatePerspectiveOffCenter(l, r, b, t, nearPlane, farPlane);
			}
#endif

			Vector3 roomSize = ROOM_BOUNDS.v;
			m_room->Draw(Matrix::CreateTranslation({0,roomSize.y/2,0}), m_view, m_proj, Colors::White, m_roomTex.Get(), false, nullptr);

			m_shapeEffect->SetView(m_view);
			m_shapeEffect->SetProjection(m_proj);
			m_world = Matrix::CreateRotationY((float)m_timer.GetTotalSeconds()*0.5f)*Matrix::CreateTranslation(-0.0f, 1.2f, -1.50f);
			m_shapeEffect->SetWorld(m_world);
			m_shape->Draw(m_shapeEffect.get(), m_shapeInputLayout.Get());

#ifdef USE_PVR
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
#endif

			m_eyeTexs[i]->commit();
#ifdef USE_PVR
			eyeLayer.ColorTexture[i] = m_eyeTexs[i]->swapChain();
			eyeLayer.Fov[i] = eyeRenderInfo[i].Fov;
			eyeLayer.RenderPose[i] = eyePoses[i];
			eyeLayer.Viewport[i] = { 0, 0,m_eyeTexs[i]->getWidth(), m_eyeTexs[i]->getHeight() };
#else
			eyeViews[i].subImage.swapchain = m_eyeTexs[i]->swapChain();
			eyeViews[i].subImage.imageArrayIndex = 0;
			eyeViews[i].subImage.imageRect = { {0, 0}, {m_eyeTexs[i]->getWidth(), m_eyeTexs[i]->getHeight()} };
			eyeViews[i].pose = views[i].pose;
			eyeViews[i].fov = views[i].fov;
#endif
		}
		
#ifdef USE_PVR
		eyeLayer.Header.Type = pvrLayerType_EyeFov;
		eyeLayer.Header.Flags = 0;
		eyeLayer.SensorSampleTime = trackingState.HeadPose.TimeInSeconds;
#else
		eyeLayer.space = m_xrLocalSpace;
#endif
	}

#ifdef USE_PVR
	pvrLayerQuad quadLayer;
	quadLayer.Header.Type = pvrLayerType_Disabled;
#else
	XrCompositionLayerQuad quadLayer{ XR_TYPE_COMPOSITION_LAYER_QUAD };
#endif
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
#ifdef USE_PVR
		quadLayer.Header.Type = pvrLayerType_Quad;
		quadLayer.Header.Flags = pvrLayerFlag_HeadLocked;
		quadLayer.ColorTexture = m_quadTex->swapChain();
		quadLayer.QuadPoseCenter.Position = { 0.0,0.0,-3.0f };
		quadLayer.QuadPoseCenter.Orientation = { 0,0,0,1.0f };
		quadLayer.QuadSize = m_quadSize;
		quadLayer.Viewport = { 0, 0,m_quadTex->getWidth(), m_quadTex->getHeight() };
#else
		quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
		quadLayer.space = m_xrViewSpace;
		quadLayer.subImage.swapchain = m_quadTex->swapChain();
		quadLayer.subImage.imageArrayIndex = 0;
		quadLayer.subImage.imageRect = { {0, 0}, {m_quadTex->getWidth(), m_quadTex->getHeight()} };
		quadLayer.pose.position = { 0.0,0.0,-3.0f };
		quadLayer.pose.orientation = { 0,0,0,1.0f };
		quadLayer.size = m_quadSize;
#endif
	}
    m_deviceResources->PIXEndEvent();

#ifdef USE_PVR
	pvrLayerHeader* layers[] = { &eyeLayer.Header,&quadLayer.Header };
#if 1
	CpuTimer t1;
	t1.start();
#endif
	ret = pvr_endFrame(m_pvrSession, 0, layers, _countof(layers));
#if 1
	t1.stop();
	Log("pvr_endFrame: %llu us\n", t1.query());
#endif
	if (ret != pvr_success) {
		pvr_logMessage(m_pvrEnv, pvrLogLevel::pvrErr, "pvr_submitFrame failed.");
	}
#else
	XrFrameEndInfo frameInfo{ XR_TYPE_FRAME_END_INFO };
	frameInfo.displayTime = frameState.predictedDisplayTime;
	frameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	XrCompositionLayerBaseHeader* layers[2] = {(XrCompositionLayerBaseHeader *)&eyeLayer, (XrCompositionLayerBaseHeader*)&quadLayer};
	frameInfo.layers = layers;
	frameInfo.layerCount = m_showQuad ? 2 : 1;
	CHECK_XRCMD(xrEndFrame(m_xrSession, &frameInfo));
#endif

#ifdef USE_PVR
	if (m_mirrorTex) {
		context->CopyResource(m_deviceResources->GetRenderTarget(), m_mirrorTex.Get());
	}
#endif

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

#ifdef USE_PVR
	m_quadTex = PvrTextureSwapChains::create(pvrTextureFormat::PVR_FORMAT_B8G8R8A8_UNORM_SRGB, 300, 200, m_pvrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);
#else
	m_quadTex = PvrTextureSwapChains::create(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, 300, 200, m_xrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);
#endif
	m_quadCenter.x = m_quadTex->getWidth() / 2.f;
	m_quadCenter.y = m_quadTex->getHeight() / 2.f;
	m_quadScreenRect.left = 0;
	m_quadScreenRect.top = 0;
	m_quadScreenRect.right = m_quadTex->getWidth();
	m_quadScreenRect.bottom = m_quadTex->getHeight();
#ifdef USE_PVR
	m_quadSize.y = 1.0f;
	m_quadSize.x = m_quadSize.y*(float)m_quadTex->getWidth() / m_quadTex->getHeight();
#else
	m_quadSize.height = 1.0f;
	m_quadSize.width = m_quadSize.height * (float)m_quadTex->getWidth() / m_quadTex->getHeight();
#endif
	
#ifdef USE_PVR
	pvrEyeRenderInfo eyeRenderInfo[pvrEye_Count];
#else
	XrViewConfigurationView views[2]{ {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} };
	uint32_t count;
	CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_xrInstance, m_xrSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &count, views));
#endif
	for (int i = 0; i < pvrEye_Count; i++)
	{
#ifdef USE_PVR
		auto eye = (i == 0 ? pvrEye_Left : pvrEye_Right);
		ExitIfFailed(pvr_getEyeRenderInfo(m_pvrSession, eye, &eyeRenderInfo[eye]));
		pvrSizei texSize;
		auto ret = pvr_getFovTextureSize(m_pvrSession, eye, eyeRenderInfo[eye].Fov, 1.0f, &texSize);
		if (ret != pvr_success) {
			pvr_logMessage(m_pvrEnv, pvrErr, "pvr_getFovTextureSize failed.");
			return;
		}
#endif

#ifdef USE_PVR
		m_eyeTexs[i] = PvrTextureSwapChains::create(pvrTextureFormat::PVR_FORMAT_B8G8R8A8_UNORM_SRGB, texSize.w, texSize.h, m_pvrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);
#else
		m_eyeTexs[i] = PvrTextureSwapChains::create(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, views[i].recommendedImageRectWidth, views[i].recommendedImageRectHeight, m_xrSession, device, DXGI_FORMAT_B8G8R8A8_UNORM);
#endif

		// Create a depth stencil view for use with 3D rendering if needed.
		CD3D11_TEXTURE2D_DESC depthStencilDesc(
			DXGI_FORMAT_D32_FLOAT,
#ifdef USE_PVR
			texSize.w,
			texSize.h,
#else
			views[i].recommendedImageRectWidth,
			views[i].recommendedImageRectHeight,
#endif
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
#ifdef USE_PVR
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
#endif
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
#ifdef USE_PVR
	m_mirrorTex.Reset();
	if (m_mirrorTexSwapChain) {
		pvr_destroyMirrorTexture(m_pvrSession, m_mirrorTexSwapChain);
	}
#endif
	m_room.reset();
	m_roomTex.Reset();

}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
