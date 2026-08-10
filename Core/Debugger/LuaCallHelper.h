#pragma once
#include "pch.h"
#include "Lua/lua.hpp"

template<typename T>
struct Nullable
{
	bool HasValue = false;
	T Value = {};
};

class LuaCallHelper
{
private:
	int _stackSize = 0;
	int _paramCount = 0;
	int _returnCount = 0;
	lua_State* _lua;

public:
	LuaCallHelper(lua_State* lua);

	void ForceParamCount(int paramCount);
	bool CheckParamCount(int minParamCount = -1);
	bool CheckSpecificParamCount(int count, int minParamCount = -1);

	void SkipParam() { _paramCount++; }

	double ReadDouble();
	bool ReadBool(bool defaultValue = false);
	uint32_t ReadInteger(uint32_t defaultValue = 0);
	uint32_t ReadIntegerFromIndex(int32_t index);
	string ReadString();
	int GetReference();

	Nullable<bool> ReadOptionalBool();
	Nullable<int32_t> ReadOptionalInteger();

	void Return(bool value);
	void Return(int value);
	void Return(uint32_t value);
	void Return(uint64_t value);
	void Return(string value);

	int ReturnCount();
};