#include "pch.h"
#include "BiquadFilter.h"
#include <cmath>

void BiquadFilter::Init(double srcRate, double dstRate, double q)
{
	if(dstRate >= srcRate) {
		//Disable filter when no downsampling is done
		_b0 = 1;
		_b1 = 0;
		_b2 = 0;
		_a1 = 0;
		_a2 = 0;
		return;
	}

	constexpr double PI = 3.14159265358979323846;
	double cutoffFreq = dstRate / 2.0;
	double w = 2.0 * PI * cutoffFreq / srcRate;
	double s = std::sin(w);
	double c = std::cos(w);

	double alpha = s / (2.0 * q);

	double a0 = 1.0 + alpha;
	_b0 = ((1.0 - c) / 2.0) / a0;
	_b1 = (1.0 - c) / a0;
	_b2 = ((1.0 - c) / 2.0) / a0;
	_a1 = (-2.0 * c) / a0;
	_a2 = (1.0 - alpha) / a0;
}

double BiquadFilter::ProcessLeft(double in)
{
	double out = (in * _b0) + _d0Left;
	_d0Left = (in * _b1) - (out * _a1) + _d1Left;
	_d1Left = (in * _b2) - (out * _a2);
	return out;
}

double BiquadFilter::ProcessRight(double in)
{
	double out = (in * _b0) + _d0Right;
	_d0Right = (in * _b1) - (out * _a1) + _d1Right;
	_d1Right = (in * _b2) - (out * _a2);
	return out;
}

void BiquadFilter::Reset()
{
	_d0Left = 0;
	_d1Left = 0;
	_d0Right = 0;
	_d1Right = 0;
}
