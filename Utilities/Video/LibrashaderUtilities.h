#pragma once
#include "pch.h"
#include "Utilities/Video/librashader_ld.h"
#include "Utilities/VirtualFile.h"
#include "Utilities/StringUtilities.h"

#ifdef _WIN32
	#include <VersionHelpers.h>
#endif

struct ShaderParamDefinition
{
	char Name[200];
	char Description[200];
	double Min;
	double Max;
	double Initial;
	double Step;
};

class LibrashaderUtilities
{
public:
	static bool IsShaderSupportEnabled()
	{
#ifdef _WIN32
		return IsWindows10OrGreater();
#elif __APPLE__
		return false;
#else
		return true;
#endif
	}

	static bool CheckShaderSupport()
	{
		static bool initialized = false;
		static bool checkResult = false;

		if(initialized) {
			return checkResult;
		}

		if(!IsShaderSupportEnabled()) {
			checkResult = false;
		} else {
			libra_instance_t libra = librashader_load_instance();
			checkResult = libra.instance_loaded;
		}
		return checkResult;
	}

	static uint32_t GetShaderParamCount(const char* shaderFile)
	{
		if(!IsShaderSupportEnabled()) {
			return 0;
		}

		libra_instance_t libra = librashader_load_instance();
		if(!libra.instance_loaded || !((VirtualFile)shaderFile).IsValid()) {
			return 0;
		}

		libra_shader_preset_t preset;
		libra_error_t error = libra.preset_create_with_options(shaderFile, nullptr, nullptr, &preset);
		if(!error) {
			libra_preset_param_list_t paramList;
			libra.preset_get_runtime_params(&preset, &paramList);
			uint32_t paramCount = paramList.length;
			libra.preset_free_runtime_params(paramList);
			libra.preset_free(&preset);
			return paramCount;
		}
		return 0;
	}

	static vector<ShaderParamDefinition> GetShaderParams(const char* shaderFile)
	{
		if(!IsShaderSupportEnabled()) {
			return {};
		}

		libra_instance_t libra = librashader_load_instance();
		if(!libra.instance_loaded || !((VirtualFile)shaderFile).IsValid()) {
			return {};
		}

		vector<ShaderParamDefinition> result;
		libra_shader_preset_t preset;
		libra_error_t error = libra.preset_create_with_options(shaderFile, nullptr, nullptr, &preset);
		if(!error) {
			libra_preset_param_list_t paramList;
			libra.preset_get_runtime_params(&preset, &paramList);

			for(uint64_t i = 0; i < paramList.length; i++) {
				const libra_preset_param_t& p = paramList.parameters[i];

				ShaderParamDefinition param = {};
				param.Max = p.maximum;
				param.Min = p.minimum;
				param.Initial = p.initial;
				param.Step = p.step;

				string name = p.name;
				string desc = p.description;
				StringUtilities::CopyToBuffer(name, param.Name, 199);
				StringUtilities::CopyToBuffer(desc, param.Description, 199);
				result.push_back(param);
			}

			libra.preset_free_runtime_params(paramList);
			libra.preset_free(&preset);
		}

		return result;
	}
};