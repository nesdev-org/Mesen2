#pragma once

#include "Common.h"
#include "Core/Shared/Interfaces/IRenderingDevice.h"
#include "Utilities/SimpleLock.h"

#define LIBRA_RUNTIME_D3D11
#include "Utilities/Video/librashader_ld.h"

using namespace DirectX;

class Emulator;

struct HudRenderInfo
{
	ID3D11Texture2D* Texture = nullptr;
	ID3D11ShaderResourceView* Shader = nullptr;
	uint32_t Width = 0;
	uint32_t Height = 0;
};

class Renderer final : public IRenderingDevice
{
private:
	Emulator* _emu;

	HWND _hWnd = nullptr;

	ID3D11Device* _pd3dDevice = nullptr;
	ID3D11DeviceContext* _pDeviceContext = nullptr;
	IDXGISwapChain* _pSwapChain = nullptr;
	ID3D11RenderTargetView* _pRenderTargetView = nullptr;

	atomic<bool> _needFlip = false;
	uint8_t* _textureBuffer[2] = { nullptr, nullptr };
	ID3D11Texture2D* _pTexture = nullptr;
	ID3D11ShaderResourceView* _pTextureSrv = nullptr;

	ID3D11VertexShader* _pVertexShader = nullptr;
	ID3D11PixelShader* _pPixelShader = nullptr;
	ID3D11InputLayout* _pInputLayout = nullptr;
	ID3D11Buffer* _pVertexBuffer = nullptr;
	ID3D11SamplerState* _pSamplerLinear = nullptr;
	ID3D11SamplerState* _pSamplerPoint = nullptr;
	ID3D11BlendState* _pBlendState = nullptr;

	libra_instance_t _libra = {};
	libra_d3d11_filter_chain_t _filterChain = {};
	bool _shaderEnabled = false;

	HudRenderInfo _emuHud = {};
	HudRenderInfo _scriptHud = {};

	SimpleLock _frameLock;
	SimpleLock _textureLock;

	FullscreenMode _newFullscreen = FullscreenMode::Disabled;
	FullscreenMode _fullscreen = FullscreenMode::Disabled;
	uint32_t _fullscreenRefreshRate = 60;
	bool _useSrgbTextureFormat = false;
	bool _allowTearing = false;
	uint32_t _bufferCount = 1;

	uint32_t _screenWidth = 0;
	uint32_t _screenHeight = 0;

	uint32_t _realScreenHeight = 240;
	uint32_t _realScreenWidth = 256;
	uint32_t _leftMargin = 0;
	uint32_t _topMargin = 0;
	uint32_t _monitorWidth = 0;
	uint32_t _monitorHeight = 0;

	uint32_t _emuFrameHeight = 0;
	uint32_t _emuFrameWidth = 0;

	ShaderConfig _shaderCfg = {};

	atomic<int> _resetCounter = 0;

	void LogError(const char* msg, HRESULT hr);

	HRESULT InitDeviceLegacy();
	HRESULT InitDevice();
	HRESULT InitDeviceCommon();

	void InitShader();
	void UpdateShaderParams();
	void LogShaderError(const char* msg, libra_error_t error);

	void CleanupDevice();

	void SetScreenSize(uint32_t width, uint32_t height);

	ID3D11Texture2D* CreateTexture(uint32_t width, uint32_t height);
	ID3D11ShaderResourceView* GetShaderResourceView(ID3D11Texture2D* texture);
	void ResetViewport();
	void DrawScreen();

	bool CreateHudTexture(HudRenderInfo& hud, uint32_t newWidth, uint32_t newHeight);
	void DrawHud(HudRenderInfo& hud, RenderSurfaceInfo& hudSurface);

	HRESULT CreateRenderTargetView();
	void ReleaseRenderTargetView();
	HRESULT CreateEmuTextureBuffers();
	void ResetTextureBuffers();

	DXGI_FORMAT GetTextureFormat();

	template<typename T>
	void CleanupCom(T& ptr);

	void DrawTexture(ID3D11ShaderResourceView* texture, RECT& destRect);

public:
	Renderer(Emulator* emu, HWND hWnd);
	~Renderer();

	void SetFullscreenMode(FullscreenSettings settings) override;

	void Reset() override;
	void Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud) override;
	void ClearFrame() override;

	void UpdateFrame(RenderedFrame& frame) override;
};