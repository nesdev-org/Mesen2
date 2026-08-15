#pragma once
#include "pch.h"

//Low-pass biquad filter
class BiquadFilter
{
private:
	double _b0 = 0;
	double _b1 = 0;
	double _b2 = 0;

	double _a1 = 0;
	double _a2 = 0;

	double _d0Left = 0;
	double _d1Left = 0;

	double _d0Right = 0;
	double _d1Right = 0;

public:
	void Init(double srcRate, double dstRate, double q);
	void Reset();

	double ProcessLeft(double in);
	double ProcessRight(double in);
};
