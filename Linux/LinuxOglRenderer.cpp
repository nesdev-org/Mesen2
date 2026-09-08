#include "LinuxOglRenderer.h"
#include "Core/Debugger/Debugger.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Video/VideoRenderer.h"
#include "Core/Shared/Video/VideoDecoder.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Core/Shared/RenderedFrame.h"
#include "Utilities/Video/LibrashaderUtilities.h"

#define GLAD_EGL_IMPLEMENTATION
#define GLAD_GL_IMPLEMENTATION
#include "Linux/include/glad/egl.h"
#include "Linux/include/glad/gl.h"

LinuxOglRenderer::LinuxOglRenderer(Emulator* emu, void* windowHandle) : _windowHandle(windowHandle)
{
	_emu = emu;
	_frameBuffer = nullptr;
	_requiredWidth = 256;
	_requiredHeight = 240;

	_emu->GetVideoRenderer()->RegisterRenderingDevice(this);
}

LinuxOglRenderer::~LinuxOglRenderer()
{
	_emu->GetVideoRenderer()->UnregisterRenderingDevice(this);

	Cleanup();
	delete[] _frameBuffer;
}

void LinuxOglRenderer::SetFullscreenMode(FullscreenSettings settings)
{
	//TODO: Implement exclusive fullscreen for Linux
}

const void* LinuxOglRenderer::LoadEglSymbol(const char* name)
{
	const void* proc = (const void*)glad_eglGetProcAddress(name);
	if(!proc) {
		MessageManager::Log("[librashader] Symbol not found: " + string(name));
	}
	return proc;
}

bool LinuxOglRenderer::InitEglContext()
{
	if(!gladLoaderLoadEGL(NULL)) {
		MessageManager::Log("[EGL] Failed to load EGL loader.");
		return false;
	}

	_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if(_eglDisplay == EGL_NO_DISPLAY) {
		MessageManager::Log("[EGL] Failed to get display.");
		return false;
	}

	EGLint major, minor;
	if(!eglInitialize(_eglDisplay, &major, &minor)) {
		MessageManager::Log("[EGL] Failed to initialize.");
		return false;
	}

	if(!gladLoadEGL(_eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
		MessageManager::Log("[EGL] Failed to load EGL.");
		return false;
	}

	eglBindAPI(EGL_OPENGL_API);

	// clang-format off
	static const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	// clang-format on

	EGLConfig eglConfig;
	EGLint numConfigs;
	if(!eglChooseConfig(_eglDisplay, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs == 0) {
		MessageManager::Log("[EGL] Failed to find matching Config.");
		return false;
	}

	_eglSurface = eglCreateWindowSurface(_eglDisplay, eglConfig, (EGLNativeWindowType)_windowHandle, nullptr);
	if(_eglSurface == EGL_NO_SURFACE) {
		MessageManager::Log("[EGL] Failed to create window surface.");
		return false;
	}

	// clang-format off
	static const EGLint contextAttribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 3,
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
		EGL_NONE
	};
	// clang-format on

	_eglContext = eglCreateContext(_eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
	if(_eglContext == EGL_NO_CONTEXT) {
		MessageManager::Log("[EGL] Failed to create context.");
		return false;
	}

	if(!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
		MessageManager::Log("[EGL] Failed to bind context.");
		return false;
	}

	if(gladLoadGL((GLADloadfunc)eglGetProcAddress) == 0) {
		MessageManager::Log("[GL] Failed to load OpenGL.");
		return false;
	}

	eglSwapInterval(_eglDisplay, _vsyncEnabled ? 1 : 0);

	return true;
}

bool LinuxOglRenderer::Init()
{
	if(!InitEglContext()) {
		return false;
	}

	if(!InitBaseShader()) {
		return false;
	}

	_mainTexture = CreateTexture(_frameWidth, _frameHeight, _useBilinearInterpolation);
	_outTexture = CreateTexture(_screenWidth, _screenHeight, _useBilinearInterpolation);

	InitSlangShader();

	return _mainTexture != 0 && _outTexture != 0;
}

void LinuxOglRenderer::InitSlangShader()
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
				_libra.gl_filter_chain_free(&_filterChain);
				_filterChain = nullptr;
			}

			eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext);

			error = _libra.gl_filter_chain_create(&preset, LoadEglSymbol, nullptr, &_filterChain);
			if(!error) {
				UpdateShaderParams();
				_shaderEnabled = true;
			} else {
				LogShaderError("[librashader] gl_filter_chain_create failed: ", error);
			}
		} else {
			_libra.preset_free(&preset);
			LogShaderError("[librashader] preset_create_with_options failed: ", error);
		}
	}
}

void LinuxOglRenderer::LogShaderError(const char* msg, libra_error_t error)
{
	char* errorMsg;
	_libra.error_write(error, &errorMsg);
	MessageManager::Log(msg + string(errorMsg));
	_libra.error_free(&error);
}

void LinuxOglRenderer::UpdateShaderParams()
{
	for(ShaderParam& param : _shaderCfg.Params) {
		_libra.gl_filter_chain_set_param(&_filterChain, param.Name, param.Value);
	}
}

bool LinuxOglRenderer::InitBaseShader()
{
	const char* vertexShaderSrc = R"(
		#version 330 core
		layout (location = 0) in vec2 aPos;
		layout (location = 1) in vec2 aTexCoord;

		out vec2 TexCoord;

		void main() {
			gl_Position = vec4(aPos, 0.0, 1.0);
			TexCoord = aTexCoord;
		}
	)";

	const char* fragmentShaderSrc = R"(
		#version 330 core
		out vec4 FragColor;
		in vec2 TexCoord;

		uniform sampler2D screenTexture;

		void main() {
			FragColor = texture(screenTexture, TexCoord);
		}
	)";

	if(!_baseShader.Compile(vertexShaderSrc, fragmentShaderSrc)) {
		MessageManager::Log("[OpenGL] Failed to compile shader.");
		return false;
	}

	// clang-format off
	float vertices[] = {
		-1.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.0f,
		1.0f, -1.0f, 1.0f, 1.0f,
	};
	// clang-format on

	glGenVertexArrays(1, &_quadVao);
	glGenBuffers(1, &_quadVbo);

	glBindVertexArray(_quadVao);

	glBindBuffer(GL_ARRAY_BUFFER, _quadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	return true;
}

GLuint LinuxOglRenderer::CreateTexture(uint32_t width, uint32_t height, bool useBilinearInterpolation)
{
	GLuint texID = 0;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	GLint filter = useBilinearInterpolation ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

	return texID;
}

void LinuxOglRenderer::Cleanup()
{
	if(_filterChain) {
		_libra.gl_filter_chain_free(&_filterChain);
		_filterChain = nullptr;
	}

	if(_mainTexture) {
		glDeleteTextures(1, &_mainTexture);
		_mainTexture = 0;
	}
	if(_outTexture) {
		glDeleteTextures(1, &_outTexture);
		_outTexture = 0;
	}
	if(_emuHud.TextureID) {
		glDeleteTextures(1, &_emuHud.TextureID);
		_emuHud.TextureID = 0;
	}
	if(_scriptHud.TextureID) {
		glDeleteTextures(1, &_scriptHud.TextureID);
		_scriptHud.TextureID = 0;
	}

	if(_quadVao) {
		glDeleteVertexArrays(1, &_quadVao);
		_quadVao = 0;
	}
	if(_quadVbo) {
		glDeleteBuffers(1, &_quadVbo);
		_quadVbo = 0;
	}

	if(_eglDisplay != EGL_NO_DISPLAY) {
		eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if(_eglContext != EGL_NO_CONTEXT) {
			eglDestroyContext(_eglDisplay, _eglContext);
			_eglContext = EGL_NO_CONTEXT;
		}
		if(_eglSurface != EGL_NO_SURFACE) {
			eglDestroySurface(_eglDisplay, _eglSurface);
			_eglSurface = EGL_NO_SURFACE;
		}
		eglTerminate(_eglDisplay);
		_eglDisplay = EGL_NO_DISPLAY;
	}
}

void LinuxOglRenderer::OnRendererThreadStarted()
{
	eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext);
}

void LinuxOglRenderer::OnRendererThreadStopped()
{
	eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void LinuxOglRenderer::Reset()
{
	Cleanup();
	if(Init()) {
		_emu->GetVideoRenderer()->RegisterRenderingDevice(this);
	} else {
		MessageManager::Log("[Video] OpenGL initialization failed.");
		Cleanup();
	}
}

void LinuxOglRenderer::SetScreenSize(uint32_t width, uint32_t height)
{
	VideoConfig cfg = _emu->GetSettings()->GetVideoConfig();
	FrameInfo size = _emu->GetVideoRenderer()->GetRendererSize();

	bool needsShaderUpdate = _emu->GetSettings()->NeedsShaderUpdate(_shaderCfg.ConfigVersion);
	bool needsShaderReload = false;
	if(needsShaderUpdate) {
		ShaderConfig shaderCfg = _emu->GetSettings()->GetShaderConfig();
		needsShaderReload = _shaderCfg.ShaderFile != shaderCfg.ShaderFile;
		_shaderCfg = shaderCfg;
	}

	if(needsShaderUpdate && !needsShaderReload) {
		UpdateShaderParams();
	}

	if(_screenHeight != size.Height || _screenWidth != size.Width ||
		_frameHeight != height || _frameWidth != width ||
		_useBilinearInterpolation != cfg.UseBilinearInterpolation ||
		_vsyncEnabled != cfg.VerticalSync || needsShaderReload) {
		_vsyncEnabled = cfg.VerticalSync;
		_useBilinearInterpolation = cfg.UseBilinearInterpolation;

		_frameHeight = height;
		_frameWidth = width;

		_screenHeight = size.Height;
		_screenWidth = size.Width;

		auto frameLock = _frameLock.AcquireSafe();
		if(!_mainTexture || !_outTexture || needsShaderReload) {
			Reset();
		} else {
			//Don't need to reset everything, only resize the textures
			//Reset() can be slow if a shader is active
			glDeleteTextures(1, &_mainTexture);
			glDeleteTextures(1, &_outTexture);
			_mainTexture = CreateTexture(_frameWidth, _frameHeight, _useBilinearInterpolation);
			_outTexture = CreateTexture(_screenWidth, _screenHeight, _useBilinearInterpolation);
		}
	}
}

void LinuxOglRenderer::ClearFrame()
{
	auto lock = _frameLock.AcquireSafe();
	if(_frameBuffer == nullptr) {
		return;
	}

	memset(_frameBuffer, 0, _requiredWidth * _requiredHeight * sizeof(uint32_t));
}

void LinuxOglRenderer::UpdateFrame(RenderedFrame& frame)
{
	auto lock = _frameLock.AcquireSafe();
	if(_frameBuffer == nullptr || _requiredWidth != frame.Width || _requiredHeight != frame.Height) {
		_requiredWidth = frame.Width;
		_requiredHeight = frame.Height;

		delete[] _frameBuffer;
		_frameBuffer = new uint32_t[frame.Width * frame.Height];
		memset(_frameBuffer, 0, frame.Width * frame.Height * 4);
	}

	memcpy(_frameBuffer, frame.FrameBuffer, frame.Width * frame.Height * sizeof(uint32_t));
}

bool LinuxOglRenderer::UpdateHudSize(HudRenderInfo& hud, uint32_t width, uint32_t height)
{
	if(!hud.TextureID || hud.Width != width || hud.Height != height) {
		if(hud.TextureID) {
			glDeleteTextures(1, &hud.TextureID);
		}
		hud.Width = width;
		hud.Height = height;
		hud.TextureID = CreateTexture(width, height, false);
		return true;
	}
	return false;
}

void LinuxOglRenderer::UpdateTextureSubImage(GLuint textureId, uint32_t width, uint32_t height, const void* src)
{
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, src);
}

void LinuxOglRenderer::DrawTexture(GLuint texture)
{
	if(texture) {
		glBindTexture(GL_TEXTURE_2D, texture);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}

GLuint LinuxOglRenderer::ProcessSlangShader()
{
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	libra_viewport_t viewport = { 0.0f, 0.0f, _screenWidth, _screenHeight };

	libra_image_gl_t in_image = {};
	in_image.handle = _mainTexture;
	in_image.format = GL_RGBA8;
	in_image.width = _frameWidth;
	in_image.height = _frameHeight;

	libra_image_gl_t out_image = {};
	out_image.handle = _outTexture;
	out_image.format = GL_RGBA8;
	out_image.width = _screenWidth;
	out_image.height = _screenHeight;

	libra_error_t error = _libra.gl_filter_chain_frame(&_filterChain, _emu->GetFrameCount(), in_image, out_image, &viewport, nullptr, nullptr);

	if(!error) {
		return _outTexture;
	} else {
		LogShaderError("[librashader] gl_filter_chain_frame failed: ", error);
		return _mainTexture;
	}
}

void LinuxOglRenderer::Render(RenderSurfaceInfo& emuHud, RenderSurfaceInfo& scriptHud)
{
	SetScreenSize(_requiredWidth, _requiredHeight);
	if(_eglContext == EGL_NO_CONTEXT || !_mainTexture) {
		return;
	}

	bool needUpdate = false;
	needUpdate |= UpdateHudSize(_emuHud, emuHud.Width, emuHud.Height);
	needUpdate |= UpdateHudSize(_scriptHud, scriptHud.Width, scriptHud.Height);

	glViewport(0, 0, _screenWidth, _screenHeight);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	{
		auto frameLock = _frameLock.AcquireSafe();
		if(_frameBuffer && _frameWidth == _requiredWidth && _frameHeight == _requiredHeight) {
			UpdateTextureSubImage(_mainTexture, _frameWidth, _frameHeight, _frameBuffer);
		}
	}

	if(needUpdate || emuHud.IsDirty) {
		UpdateTextureSubImage(_emuHud.TextureID, _emuHud.Width, _emuHud.Height, emuHud.Buffer);
	}
	if(needUpdate || scriptHud.IsDirty) {
		UpdateTextureSubImage(_scriptHud.TextureID, _scriptHud.Width, _scriptHud.Height, scriptHud.Buffer);
	}

	GLuint textureToDraw = _mainTexture;
	if(_shaderEnabled && _filterChain) {
		textureToDraw = ProcessSlangShader();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, _screenWidth, _screenHeight);

	_baseShader.Bind();
	glBindVertexArray(_quadVao);

	glActiveTexture(GL_TEXTURE0);
	DrawTexture(textureToDraw);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	DrawTexture(_scriptHud.TextureID);
	DrawTexture(_emuHud.TextureID);
	glDisable(GL_BLEND);

	eglSwapBuffers(_eglDisplay, _eglSurface);
}