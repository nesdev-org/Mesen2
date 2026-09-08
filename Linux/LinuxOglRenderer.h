#pragma once
#include "Core/Shared/Interfaces/IRenderingDevice.h"
#include "Core/Shared/SettingTypes.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/RenderedFrame.h"
#include "Utilities/SimpleLock.h"

#include "Linux/LinuxOglShader.h"
#define LIBRA_RUNTIME_OPENGL
#include "Utilities/Video/librashader_ld.h"

#include "Linux/include/glad/egl.h"
#include "Linux/include/glad/gl.h"

struct HudRenderInfo
{
	GLuint TextureID = 0;
	uint32_t Width = 0;
	uint32_t Height = 0;
};

class LinuxOglRenderer : public IRenderingDevice
{
private:
	Emulator* _emu = nullptr;

	void* _windowHandle = nullptr;

	EGLDisplay _eglDisplay = EGL_NO_DISPLAY;
	EGLSurface _eglSurface = EGL_NO_SURFACE;
	EGLContext _eglContext = EGL_NO_CONTEXT;

	GLuint _mainTexture = 0;
	GLuint _outTexture = 0;

	GLuint _quadVao = 0;
	GLuint _quadVbo = 0;
	LinuxOglShader _baseShader = {};

	HudRenderInfo _emuHud = {};
	HudRenderInfo _scriptHud = {};

	bool _useBilinearInterpolation = false;

	SimpleLock _frameLock;
	uint32_t* _frameBuffer = nullptr;

	uint32_t _screenWidth = 0;
	uint32_t _screenHeight = 0;

	uint32_t _requiredHeight = 0;
	uint32_t _requiredWidth = 0;

	uint32_t _frameHeight = 0;
	uint32_t _frameWidth = 0;

	bool _vsyncEnabled = false;

	libra_instance_t _libra = {};
	libra_gl_filter_chain_t _filterChain = nullptr;
	ShaderConfig _shaderCfg = {};
	bool _shaderEnabled = false;

	void InitSlangShader();
	void LogShaderError(const char* msg, libra_error_t error);
	void UpdateShaderParams();

	bool Init();
	void Cleanup();
	void SetScreenSize(uint32_t width, uint32_t height);

	GLuint ProcessSlangShader();
	void DrawTexture(GLuint texture);

	bool UpdateHudSize(HudRenderInfo& hud, uint32_t width, uint32_t height);
	void UpdateHudTexture(HudRenderInfo& hud, uint32_t* src);

	bool InitEglContext();
	bool InitBaseShader();
	GLuint CreateTexture(uint32_t width, uint32_t height, bool useBilinearInterpolation);
	void UpdateTextureSubImage(GLuint textureId, uint32_t width, uint32_t height, const void* src);

	static const void* LoadEglSymbol(const char* name);

public:
	LinuxOglRenderer(Emulator* emu, void* windowHandle);
	virtual ~LinuxOglRenderer();

	void ClearFrame() override;
	void UpdateFrame(RenderedFrame& frame) override;
	void Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud) override;
	void Reset() override;
	void OnRendererThreadStarted() override;
	void OnRendererThreadStopped() override;

	void SetFullscreenMode(FullscreenSettings settings) override;
};
