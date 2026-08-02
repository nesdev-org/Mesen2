#include "pch.h"
#include "UTF8Util.h"
#ifdef __APPLE__
	#include <codecvt>
	#include <locale>
#else
	#include <cuchar>
#endif

#ifdef _MSC_VER
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#undef WIN32_LEAN_AND_MEAN
#endif

namespace utf8
{
	std::wstring utf8::decode(const std::string& str)
	{
#ifdef _MSC_VER
		std::wstring ret;
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
		if(len > 0) {
			ret.resize(len);
			MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &ret[0], len);
		}
		return ret;
#else
		throw std::runtime_error("utf8::decode is not implemented for this platform");
#endif
	}

	string utf8::encode(const std::wstring& wstr)
	{
#ifdef _WIN32
		return encode(std::u16string((char16_t*)wstr.c_str()));
#else
		throw std::runtime_error("utf8::encode(wstring) is not implemented for this platform");
#endif
	}

	string utf8::encode(const std::u16string& wstr)
	{
#ifdef __APPLE__
		std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
		return conv.to_bytes(wstr);
#else
		std::mbstate_t state {};
		string result;
		result.reserve(wstr.size());

		char buffer[10];

		for(char16_t c16 : wstr) {
			size_t size = std::c16rtomb(buffer, c16, &state);
			if(size == (size_t)-1) {
				//Ignore invalid characters
				continue;
			} else if(size > 0) {
				result.append(buffer, size);
			}
		}

		return result;
#endif
	}
}