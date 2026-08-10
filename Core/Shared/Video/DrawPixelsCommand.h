#pragma once
#include "pch.h"
#include "Shared/Video/DrawCommand.h"

class DrawPixelsCommand : public DrawCommand
{
private:
	int _x, _y, _width, _height;
	uint32_t* _data = nullptr;

protected:
	void InternalDraw()
	{
		int pos = 0;
		for(int i = 0; i < _height; i++) {
			for(int j = 0; j < _width; j++) {
				DrawPixel(_x + j, _y + i, _data[pos] ^ 0xFF000000);
				pos++;
			}
		}
	}

public:
	DrawPixelsCommand(uint32_t* data, int x, int y, int width, int height, int frameCount, int startFrame) : DrawCommand(startFrame, frameCount), _x(x), _y(y), _width(width), _height(height)
	{
		_data = data;
	}

	~DrawPixelsCommand()
	{
		delete[] _data;
	}
};
