#include "Common.h"
#include "Renderer.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/MessageManager.h"
#include "Core/Shared/SettingTypes.h"
#include "Core/Shared/EmuSettings.h"
#define LIBRA_RUNTIME_D3D11
#include "Utilities/Video/librashader_ld.h"
#include "Utilities/Video/LibrashaderUtilities.h"
#include <wrl/client.h>
#include <dxgi1_6.h>

#define CheckError(msg) if(FAILED(hr)) { LogError(msg, hr); return hr; }

using namespace DirectX;
using Microsoft::WRL::ComPtr;

#ifdef _DEBUG
static UINT createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
static UINT createDeviceFlags = 0;
#endif

static D3D_DRIVER_TYPE driverTypes[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP, D3D_DRIVER_TYPE_REFERENCE };
static UINT numDriverTypes = ARRAYSIZE(driverTypes);

Renderer::Renderer(Emulator* emu, HWND hWnd)
{
	_emu = emu;
	_hWnd = hWnd;

	SetScreenSize(256, 224);
}

Renderer::~Renderer()
{
	VideoRenderer* videoRenderer = _emu->GetVideoRenderer();
	if(videoRenderer) {
		videoRenderer->UnregisterRenderingDevice(this);
	}
	CleanupDevice();
}

void Renderer::SetFullscreenMode(FullscreenSettings settings)
{
	if(settings.Mode != _fullscreen || _hWnd != (HWND)settings.WindowHandle) {
		int counter = _resetCounter;

		_hWnd = (HWND)settings.WindowHandle;
		_monitorWidth = settings.Width;
		_monitorHeight = settings.Height;

		_newFullscreen = settings.Mode;

		while(_resetCounter <= counter) {
			std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(10));
		}
	}
}

DXGI_FORMAT Renderer::GetTextureFormat()
{
	return _useSrgbTextureFormat ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
}

void Renderer::SetScreenSize(uint32_t width, uint32_t height)
{
	VideoConfig& cfg = _emu->GetSettings()->GetVideoConfig();
	FrameInfo rendererSize = _emu->GetVideoRenderer()->GetRendererSize();
	uint32_t refreshRate = _emu->GetFps() < 55 ? cfg.ExclusiveFullscreenRefreshRatePal : cfg.ExclusiveFullscreenRefreshRateNtsc;

	bool needsShaderUpdate = _emu->GetSettings()->NeedsShaderUpdate(_shaderCfg.ConfigVersion);
	bool needsShaderReload = false;
	if(needsShaderUpdate) {
		ShaderConfig shaderCfg = _emu->GetSettings()->GetShaderConfig();
		needsShaderReload = _shaderCfg.ShaderFile != shaderCfg.ShaderFile;
		_shaderCfg = shaderCfg;
	}

	auto needUpdate = [=] {
		return (
			_emuFrameHeight != height ||
			_emuFrameWidth != width ||
			_screenHeight != rendererSize.Height ||
			_screenWidth != rendererSize.Width ||
			_newFullscreen != _fullscreen ||
			needsShaderReload ||
			_useSrgbTextureFormat != cfg.UseSrgbTextureFormat ||
			(_fullscreen == FullscreenMode::Exclusive && _fullscreenRefreshRate != refreshRate) ||
			(_fullscreen != FullscreenMode::Disabled && (_realScreenHeight != _monitorHeight || _realScreenWidth != _monitorWidth)));
	};

	if(needsShaderUpdate && !needsShaderReload) {
		auto frameLock = _frameLock.AcquireSafe();
		UpdateShaderParams();
	}

	if(needUpdate()) {
		auto frameLock = _frameLock.AcquireSafe();
		auto textureLock = _textureLock.AcquireSafe();
		if(needUpdate()) {
			_emuFrameHeight = height;
			_emuFrameWidth = width;

			bool needReset = _fullscreen != _newFullscreen || needsShaderReload;
			bool fullscreenResizeMode = _fullscreen != FullscreenMode::Disabled && _newFullscreen != FullscreenMode::Disabled;

			if(_pSwapChain && _fullscreen == FullscreenMode::Exclusive && _newFullscreen != FullscreenMode::Exclusive) {
				HRESULT hr = _pSwapChain->SetFullscreenState(FALSE, NULL);
				if(FAILED(hr)) {
					LogError("SetFullscreenState(FALSE) failed.", hr);
				}
			}

			if(_useSrgbTextureFormat != cfg.UseSrgbTextureFormat) {
				_useSrgbTextureFormat = cfg.UseSrgbTextureFormat;
				needReset = true;
			}

			_fullscreen = _newFullscreen;
			if(_fullscreenRefreshRate != refreshRate) {
				_fullscreenRefreshRate = refreshRate;
				needReset = true;
			}

			_screenHeight = rendererSize.Height;
			_screenWidth = rendererSize.Width;

			if(_fullscreen != FullscreenMode::Disabled) {
				if(_realScreenHeight != _monitorHeight) {
					_realScreenHeight = _monitorHeight;
					needReset = true;
				}

				if(_realScreenWidth != _monitorWidth) {
					_realScreenWidth = _monitorWidth;
					needReset = true;
				}

				//Ensure the screen width/height is smaller or equal to the fullscreen resolution, no matter the requested scale
				if(_monitorHeight < _screenHeight || _monitorWidth < _screenWidth) {
					double scale = (double)width / (double)height;
					_screenHeight = _monitorHeight;
					_screenWidth = (uint32_t)(scale * _screenHeight);
					if(_monitorWidth < _screenWidth) {
						_screenWidth = _monitorWidth;
						_screenHeight = (uint32_t)(_screenWidth / scale);
					}
				}
			} else {
				_realScreenHeight = _screenHeight;
				_realScreenWidth = _screenWidth;
			}

			_leftMargin = (_realScreenWidth - _screenWidth) / 2;
			_topMargin = (_realScreenHeight - _screenHeight) / 2;

			if(!_pSwapChain || needReset) {
				Reset();
			} else {
				if(fullscreenResizeMode) {
					ResetTextureBuffers();
					CreateEmuTextureBuffers();
				} else {
					ResetTextureBuffers();
					ReleaseRenderTargetView();
					_pSwapChain->ResizeBuffers(_bufferCount, _realScreenWidth, _realScreenHeight, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
					CreateRenderTargetView();
					CreateEmuTextureBuffers();
				}
			}
		}
	}
}

void Renderer::Reset()
{
	auto lock = _frameLock.AcquireSafe();
	CleanupDevice();
	if(FAILED(InitDevice())) {
		CleanupDevice();
	} else {
		_emu->GetVideoRenderer()->RegisterRenderingDevice(this);
	}

	_resetCounter++;
}

template<typename T>
void Renderer::CleanupCom(T& ptr)
{
	if(ptr) {
		ptr->Release();
		ptr = nullptr;
	}
}

void Renderer::CleanupDevice()
{
	ResetTextureBuffers();
	ReleaseRenderTargetView();

	CleanupCom(_pVertexShader);
	CleanupCom(_pPixelShader);
	CleanupCom(_pInputLayout);
	CleanupCom(_pVertexBuffer);
	CleanupCom(_pSamplerLinear);
	CleanupCom(_pSamplerPoint);
	CleanupCom(_pBlendState);

	if(_pSwapChain) {
		_pSwapChain->SetFullscreenState(false, nullptr);
		CleanupCom(_pSwapChain);
	}

	if(_pDeviceContext) {
		_pDeviceContext->ClearState();
		_pDeviceContext->Flush();
		CleanupCom(_pDeviceContext);
	}

	CleanupCom(_pd3dDevice);
	CleanupCom(_emuHud.Texture);
	CleanupCom(_emuHud.Shader);
	CleanupCom(_scriptHud.Texture);
	CleanupCom(_scriptHud.Shader);

	if(_libra.d3d11_filter_chain_free && _filterChain) {
		_libra.d3d11_filter_chain_free(&_filterChain);
	}
}

void Renderer::ResetTextureBuffers()
{
	CleanupCom(_pTexture);
	CleanupCom(_pTextureSrv);

	delete[] _textureBuffer[0];
	_textureBuffer[0] = nullptr;
	delete[] _textureBuffer[1];
	_textureBuffer[1] = nullptr;
}

void Renderer::ReleaseRenderTargetView()
{
	CleanupCom(_pRenderTargetView);
}

HRESULT Renderer::CreateRenderTargetView()
{
	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	HRESULT hr = _pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	CheckError("SwapChain::GetBuffer() failed.");

	D3D11_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = GetTextureFormat();
	desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = 0;

	hr = _pd3dDevice->CreateRenderTargetView(pBackBuffer, &desc, &_pRenderTargetView);
	pBackBuffer->Release();
	CheckError("D3DDevice::CreateRenderTargetView() failed.");

	_pDeviceContext->OMSetRenderTargets(1, &_pRenderTargetView, nullptr);

	return S_OK;
}

HRESULT Renderer::CreateEmuTextureBuffers()
{
	ResetViewport();

	_textureBuffer[0] = new uint8_t[_emuFrameWidth * _emuFrameHeight * 4];
	_textureBuffer[1] = new uint8_t[_emuFrameWidth * _emuFrameHeight * 4];
	memset(_textureBuffer[0], 0, _emuFrameWidth * _emuFrameHeight * 4);
	memset(_textureBuffer[1], 0, _emuFrameWidth * _emuFrameHeight * 4);

	_pTexture = CreateTexture(_emuFrameWidth, _emuFrameHeight);
	if(!_pTexture) {
		return S_FALSE;
	}
	_pTextureSrv = GetShaderResourceView(_pTexture);
	if(!_pTextureSrv) {
		return S_FALSE;
	}
	return S_OK;
}

void Renderer::LogError(const char* msg, HRESULT hr)
{
	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		LANG_USER_DEFAULT,
		(LPSTR)&messageBuffer,
		0,
		NULL);

	string errorMsg(messageBuffer, size);
	LocalFree(messageBuffer);
	MessageManager::Log("[DX11] " + string(msg) + " Error: " + errorMsg + " (" + std::to_string(hr) + ")");
}

HRESULT Renderer::InitDeviceLegacy()
{
	//Used for Windows 7, when the "Platform Update" (KB2670838) isn't installed
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = _realScreenWidth;
	sd.BufferDesc.Height = _realScreenHeight;
	sd.BufferDesc.Format = GetTextureFormat();
	sd.BufferDesc.RefreshRate.Numerator = _fullscreenRefreshRate;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.Flags = _fullscreen == FullscreenMode::Exclusive ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;
	sd.OutputWindow = _hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	HRESULT hr = S_OK;
	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	for(UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
		driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd, &_pSwapChain, &_pd3dDevice, &featureLevel, &_pDeviceContext);
		if(SUCCEEDED(hr)) {
			break;
		}
	}

	CheckError("D3D11CreateDeviceAndSwapChain() failed.");

	return InitDeviceCommon();
}

HRESULT Renderer::InitDevice()
{
	HRESULT hr = S_OK;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	ComPtr<IDXGIFactory2> factory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if(FAILED(hr)) {
		MessageManager::Log("DX11.1 runtime not found, attempting to use DX11.0...");
		return InitDeviceLegacy();
	}

	VideoConfig& cfg = _emu->GetSettings()->GetVideoConfig();
	bool enableFlipSwap = false;
	bool allowTearing = false;

	if(cfg.EnableVariableRefreshRate && _fullscreen == FullscreenMode::Borderless) {
		ComPtr<IDXGIFactory5> factory5;
		if(SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
			enableFlipSwap = true;

			BOOL tearingSupported = FALSE;
			hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported));
			allowTearing = tearingSupported;
			if(FAILED(hr)) {
				LogError("CheckFeatureSupport() failed.", hr);
			}
		}
	}

	_allowTearing = allowTearing;
	_bufferCount = enableFlipSwap ? 2 : 1;

	DXGI_SWAP_CHAIN_DESC1 sd = {};
	sd.BufferCount = _bufferCount;
	sd.Width = _realScreenWidth;
	sd.Height = _realScreenHeight;
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = enableFlipSwap ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Flags =
		(_fullscreen == FullscreenMode::Exclusive ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0) |
		(_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC sdFullscreen = {};
	sdFullscreen.RefreshRate.Numerator = _fullscreenRefreshRate;
	sdFullscreen.RefreshRate.Denominator = 1;
	sdFullscreen.Windowed = TRUE;

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
	for(UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
		driverType = driverTypes[driverTypeIndex];
		featureLevel = D3D_FEATURE_LEVEL_11_1;

		hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &_pd3dDevice, &featureLevel, &_pDeviceContext);
		if(hr == E_INVALIDARG) {
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			featureLevel = D3D_FEATURE_LEVEL_11_0;
			hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1, D3D11_SDK_VERSION, &_pd3dDevice, &featureLevel, &_pDeviceContext);
		}

		if(FAILED(hr)) {
			LogError("D3D11CreateDevice() failed.", hr);
			continue;
		} else {
			break;
		}
	}

	if(FAILED(hr)) {
		return hr;
	}

	hr = factory->CreateSwapChainForHwnd(_pd3dDevice, _hWnd, &sd, _fullscreen == FullscreenMode::Exclusive ? &sdFullscreen : nullptr, nullptr, (IDXGISwapChain1**)&_pSwapChain);
	if(hr == DXGI_ERROR_INVALID_CALL) {
		//Retry with legacy swap effect
		_allowTearing = false;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		hr = factory->CreateSwapChainForHwnd(_pd3dDevice, _hWnd, &sd, _fullscreen == FullscreenMode::Exclusive ? &sdFullscreen : nullptr, nullptr, (IDXGISwapChain1**)&_pSwapChain);
	}

	CheckError("CreateSwapChainForHwnd() failed.");

	return InitDeviceCommon();
}

HRESULT Renderer::InitDeviceCommon()
{
	HRESULT hr = S_OK;
	if(_fullscreen == FullscreenMode::Exclusive) {
		hr = _pSwapChain->SetFullscreenState(TRUE, NULL);
		if(FAILED(hr)) {
			LogError("SetFullscreenState(true) failed.", hr);
			MessageManager::Log("Switching back to windowed mode");
			hr = _pSwapChain->SetFullscreenState(FALSE, NULL);
			CheckError("SetFullscreenState(false) failed.");
		} else {
			//Get actual monitor resolution (which might differ from the one that was requested)
			HMONITOR monitor = MonitorFromWindow(_hWnd, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO info = {};
			info.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &info);

			uint32_t monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
			uint32_t monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;

			if(_monitorHeight != monitorHeight || _monitorWidth != monitorWidth) {
				MessageManager::Log(
					"Requested resolution (" + std::to_string(_monitorWidth) + "x" + std::to_string(_monitorHeight) + ") is not available. Resetting to nearest match instead: " +
					std::to_string(monitorWidth) + "x" + std::to_string(monitorHeight));
				_monitorWidth = monitorWidth;
				_monitorHeight = monitorHeight;

				//Make UI wait until this 2nd reset is over
				_resetCounter--;
			}
		}
	}

	hr = CreateRenderTargetView();
	if(FAILED(hr)) {
		return hr;
	}
	hr = CreateEmuTextureBuffers();
	if(FAILED(hr)) {
		return hr;
	}

	string shaderCode = R"(
struct VS_IN
{
	float3 pos : POSITION;
	float2 tex : TEXCOORD;
};

struct PS_IN 
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

PS_IN VS(VS_IN input)
{ 
	PS_IN output;
	output.pos = float4(input.pos, 1.0f);
	output.tex = input.tex;
	return output;
}

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 PS(PS_IN input) : SV_Target
{
	return tex.Sample(samp, input.tex);
}
)";

	ID3DBlob* vsBlob = nullptr;
	hr = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
	CheckError("D3DCompile (Vertex Shader) failed.");
	hr = _pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_pVertexShader);
	CheckError("CreateVertexShader failed.");

	ID3DBlob* psBlob = nullptr;
	hr = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
	CheckError("D3DCompile (Pixel Shader) failed.");

	hr = _pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_pPixelShader);
	CheckError("CreatePixelShader failed.");

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	hr = _pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_pInputLayout);
	CheckError("CreateInputLayout failed.");

	vsBlob->Release();
	psBlob->Release();

	//Vertex buffer
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(float) * 5 * 4; // 4 vertices * (3 pos + 2 tex)
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = _pd3dDevice->CreateBuffer(&bd, nullptr, &_pVertexBuffer);
	CheckError("CreateBuffer failed.");

	//Samplers
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	//Used when bilinear interpolation is turned on
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = _pd3dDevice->CreateSamplerState(&sampDesc, &_pSamplerLinear);
	CheckError("CreateSampleState failed.");

	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	hr = _pd3dDevice->CreateSamplerState(&sampDesc, &_pSamplerPoint);
	CheckError("CreateSampleState failed.");

	//Blend state for HUD
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = _pd3dDevice->CreateBlendState(&blendDesc, &_pBlendState);
	CheckError("CreateBlendState failed.");

	InitShader();

	return S_OK;
}

void Renderer::InitShader()
{
	_shaderEnabled = false;

	VirtualFile shader = _shaderCfg.ShaderFile;
	if(shader.IsValid()) {
		if(!_libra.instance_loaded && LibrashaderUtilities::CheckShaderSupport()) {
			_libra = librashader_load_instance();
		}

		if(!_libra.instance_loaded) {
			return;
		}

		libra_shader_preset_t preset = {};

		libra_error_t error = _libra.preset_create_with_options(_shaderCfg.ShaderFile.c_str(), nullptr, nullptr, &preset);
		if(!error) {
			if(_filterChain) {
				_libra.d3d11_filter_chain_free(&_filterChain);
			}

			error = _libra.d3d11_filter_chain_create(&preset, _pd3dDevice, NULL, &_filterChain);
			if(!error) {
				UpdateShaderParams();
				_shaderEnabled = true;
			} else {
				LogShaderError("[librashader] d3d11_filter_chain_create failed: ", error);
			}
		} else {
			LogShaderError("[librashader] preset_create_with_options failed: ", error);
		}
	}
}

void Renderer::UpdateShaderParams()
{
	for(ShaderParam& param : _shaderCfg.Params) {
		_libra.d3d11_filter_chain_set_param(&_filterChain, param.Name, param.Value);
	}
}

void Renderer::LogShaderError(const char* msg, libra_error_t error)
{
	char* errorMsg;
	_libra.error_write(error, &errorMsg);
	MessageManager::Log(msg + string(errorMsg));
	_libra.error_free(&error);
}

ID3D11Texture2D* Renderer::CreateTexture(uint32_t width, uint32_t height)
{
	ID3D11Texture2D* texture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.Format = GetTextureFormat();
	desc.MipLevels = 1;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.Width = width;
	desc.Height = height;
	desc.MiscFlags = 0;

	HRESULT hr = _pd3dDevice->CreateTexture2D(&desc, nullptr, &texture);
	if(FAILED(hr)) {
		LogError("D3DDevice::CreateTexture() failed.", hr);
		return nullptr;
	}
	return texture;
}

ID3D11ShaderResourceView* Renderer::GetShaderResourceView(ID3D11Texture2D* texture)
{
	ID3D11ShaderResourceView* shaderResourceView = nullptr;
	HRESULT hr = _pd3dDevice->CreateShaderResourceView(texture, nullptr, &shaderResourceView);
	if(FAILED(hr)) {
		LogError("D3DDevice::CreateShaderResourceView() failed.", hr);
		return nullptr;
	}

	return shaderResourceView;
}

void Renderer::ResetViewport()
{
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)_realScreenWidth;
	vp.Height = (FLOAT)_realScreenHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	_pDeviceContext->RSSetViewports(1, &vp);
}

void Renderer::ClearFrame()
{
	//Clear current output and display black frame
	auto lock = _textureLock.AcquireSafe();
	if(_textureBuffer[0]) {
		//_textureBuffer[0] may be null if directx failed to initialize properly
		memset(_textureBuffer[0], 0, _emuFrameWidth * _emuFrameHeight * sizeof(uint32_t));
		_needFlip = true;
	}
}

void Renderer::UpdateFrame(RenderedFrame& frame)
{
	SetScreenSize(frame.Width, frame.Height);

	auto lock = _textureLock.AcquireSafe();
	if(_textureBuffer[0]) {
		//_textureBuffer[0] may be null if directx failed to initialize properly
		memcpy(_textureBuffer[0], frame.FrameBuffer, frame.Width * frame.Height * sizeof(uint32_t));
		_needFlip = true;
	}
}

void Renderer::DrawTexture(ID3D11ShaderResourceView* texture, RECT& destRect)
{
	float left = (float)destRect.left / _realScreenWidth * 2.0f - 1.0f;
	float right = (float)destRect.right / _realScreenWidth * 2.0f - 1.0f;
	float top = 1.0f - (float)destRect.top / _realScreenHeight * 2.0f;
	float bottom = 1.0f - (float)destRect.bottom / _realScreenHeight * 2.0f;

	// clang-format off
	float vertices[] = {
		left, bottom, 0.0f, 0.0f, 1.0f,
		left, top, 0.0f, 0.0f, 0.0f,
		right, bottom, 0.0f, 1.0f, 1.0f,
		right, top, 0.0f, 1.0f, 0.0f
	};
	// clang-format on

	D3D11_MAPPED_SUBRESOURCE mapped;
	_pDeviceContext->Map(_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, vertices, sizeof(vertices));
	_pDeviceContext->Unmap(_pVertexBuffer, 0);

	UINT stride = sizeof(float) * 5;
	UINT offset = 0;
	_pDeviceContext->IASetVertexBuffers(0, 1, &_pVertexBuffer, &stride, &offset);
	_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_pDeviceContext->IASetInputLayout(_pInputLayout);
	_pDeviceContext->VSSetShader(_pVertexShader, nullptr, 0);
	_pDeviceContext->PSSetShader(_pPixelShader, nullptr, 0);
	_pDeviceContext->PSSetShaderResources(0, 1, &texture);
	_pDeviceContext->Draw(4, 0);
}

void Renderer::DrawScreen()
{
	//Swap buffers - emulator always writes to _textureBuffer[0], screen always draws _textureBuffer[1]
	if(_needFlip) {
		auto lock = _textureLock.AcquireSafe();
		uint8_t* textureBuffer = _textureBuffer[0];
		_textureBuffer[0] = _textureBuffer[1];
		_textureBuffer[1] = textureBuffer;
		_needFlip = false;
	}

	//Copy buffer to texture
	uint32_t bpp = 4;
	uint32_t rowPitch = _emuFrameWidth * bpp;
	D3D11_MAPPED_SUBRESOURCE dd;
	HRESULT hr = _pDeviceContext->Map(_pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &dd);
	if(FAILED(hr)) {
		LogError("DeviceContext::Map() failed.", hr);
		return;
	}
	uint8_t* surfacePointer = (uint8_t*)dd.pData;
	uint8_t* videoBuffer = _textureBuffer[1];
	if(rowPitch != dd.RowPitch) {
		for(uint32_t i = 0, iMax = _emuFrameHeight; i < iMax; i++) {
			memcpy(surfacePointer, videoBuffer, rowPitch);
			videoBuffer += rowPitch;
			surfacePointer += dd.RowPitch;
		}
	} else {
		memcpy(surfacePointer, videoBuffer, rowPitch * _emuFrameHeight);
	}
	_pDeviceContext->Unmap(_pTexture, 0);

	if(_shaderEnabled) {
		frame_d3d11_opt_t frame_opt = {};
		libra_viewport_t viewport = {};
		viewport.x = _leftMargin;
		viewport.y = _topMargin;
		viewport.width = _screenWidth;
		viewport.height = _screenHeight;

		libra_error_t error = _libra.d3d11_filter_chain_frame(&_filterChain, _pDeviceContext, _emu->GetFrameCount(), _pTextureSrv, _pRenderTargetView, &viewport, NULL, &frame_opt);
		if(error) {
			LogShaderError("[librashader] d3d11_filter_chain_frame failed: ", error);
		}

		ResetViewport();
	} else {
		RECT destRect;
		destRect.left = _leftMargin;
		destRect.top = _topMargin;
		destRect.right = _screenWidth + _leftMargin;
		destRect.bottom = _screenHeight + _topMargin;
		DrawTexture(_pTextureSrv, destRect);
	}
}

bool Renderer::CreateHudTexture(HudRenderInfo& hud, uint32_t newWidth, uint32_t newHeight)
{
	if(hud.Texture) {
		hud.Texture->Release();
		hud.Texture = nullptr;
	}
	if(hud.Shader) {
		hud.Shader->Release();
		hud.Shader = nullptr;
	}

	hud.Width = newWidth;
	hud.Height = newHeight;

	hud.Texture = CreateTexture(hud.Width, hud.Height);
	if(!hud.Texture) {
		return false;
	}
	hud.Shader = GetShaderResourceView(hud.Texture);
	if(!hud.Shader) {
		return false;
	}

	return true;
}

void Renderer::DrawHud(HudRenderInfo& hud, RenderSurfaceInfo& hudSurface)
{
	uint32_t* hudBuffer = hudSurface.Buffer;
	uint32_t newWidth = hudSurface.Width;
	uint32_t newHeight = hudSurface.Height;

	if(newWidth == 0 && newHeight == 0) {
		return;
	}

	bool needRedraw = hudSurface.IsDirty;
	if(hud.Width != newWidth || hud.Height != newHeight || !hud.Texture || !hud.Shader) {
		needRedraw = true;
		if(!CreateHudTexture(hud, newWidth, newHeight)) {
			return;
		}
	}

	if(needRedraw) {
		//Copy buffer to texture
		uint32_t rowPitch = hud.Width * sizeof(uint32_t);
		D3D11_MAPPED_SUBRESOURCE dd;
		HRESULT hr = _pDeviceContext->Map(hud.Texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &dd);
		if(FAILED(hr)) {
			LogError("DeviceContext::Map() failed.", hr);
			return;
		}
		uint8_t* surfacePointer = (uint8_t*)dd.pData;
		uint8_t* videoBuffer = (uint8_t*)hudBuffer;
		if(rowPitch != dd.RowPitch) {
			for(uint32_t i = 0, iMax = hud.Height; i < iMax; i++) {
				memcpy(surfacePointer, videoBuffer, rowPitch);
				videoBuffer += rowPitch;
				surfacePointer += dd.RowPitch;
			}
		} else {
			memcpy(surfacePointer, videoBuffer, hud.Height * rowPitch);
		}
		_pDeviceContext->Unmap(hud.Texture, 0);
	}

	RECT destRect;
	destRect.left = _leftMargin;
	destRect.top = _topMargin;
	destRect.right = _screenWidth + _leftMargin;
	destRect.bottom = _screenHeight + _topMargin;

	DrawTexture(hud.Shader, destRect);
}

void Renderer::Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud)
{
	auto lock = _frameLock.AcquireSafe();

	if(_newFullscreen != _fullscreen) {
		SetScreenSize(_emuFrameWidth, _emuFrameHeight);
	}

	if(_pDeviceContext == nullptr) {
		//DirectX failed to initialize, try to init
		Reset();
		if(_pDeviceContext == nullptr) {
			//Can't init, prevent crash
			return;
		}
	}

	_pDeviceContext->OMSetRenderTargets(1, &_pRenderTargetView, nullptr);

	VideoConfig& cfg = _emu->GetSettings()->GetVideoConfig();

	// Clear the back buffer
	_pDeviceContext->ClearRenderTargetView(_pRenderTargetView, Colors::Black);

	_pDeviceContext->OMSetBlendState(_pBlendState, nullptr, 0xFFFFFFFF);

	//Draw screen
	ID3D11SamplerState* sampler = cfg.UseBilinearInterpolation ? _pSamplerLinear : _pSamplerPoint;
	_pDeviceContext->PSSetSamplers(0, 1, &sampler);
	DrawScreen();

	//Draw HUD
	_pDeviceContext->OMSetRenderTargets(1, &_pRenderTargetView, nullptr);
	_pDeviceContext->PSSetSamplers(0, 1, &_pSamplerPoint);
	DrawHud(_scriptHud, scriptHud);
	DrawHud(_emuHud, emuHud);

	// Present the information rendered to the back buffer to the front buffer (the screen)
	HRESULT hr = _pSwapChain->Present((cfg.VerticalSync && !_allowTearing) ? 1 : 0, _allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
	if(FAILED(hr)) {
		LogError("SwapChain::Present() failed.", hr);
		if(hr == DXGI_ERROR_DEVICE_REMOVED) {
			MessageManager::Log("D3DDevice: GetDeviceRemovedReason: " + std::to_string(_pd3dDevice->GetDeviceRemovedReason()));
		}
		MessageManager::Log("Trying to reset DX...");
		Reset();
	}
}
