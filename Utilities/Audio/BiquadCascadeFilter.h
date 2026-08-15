#pragma once
#include "pch.h"
#include "BiquadFilter.h"

//Low-pass 4th order butterworth filter implemented by running the
//signal through 2 biquad filters with different Q values
class BiquadCascadeFilter
{
private:
	BiquadFilter _stage1;
	BiquadFilter _stage2;

public:
	void Init(double srcRate, double dstRate);
	void Reset();

	double ProcessLeft(double in);
	double ProcessRight(double in);
};
