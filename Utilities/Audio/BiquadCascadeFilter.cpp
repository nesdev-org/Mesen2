#include "pch.h"
#include "BiquadCascadeFilter.h"

void BiquadCascadeFilter::Init(double srcRate, double dstRate)
{
	_stage1.Init(srcRate, dstRate, 0.54119610);
	_stage2.Init(srcRate, dstRate, 1.30656296);
}

void BiquadCascadeFilter::Reset()
{
	_stage1.Reset();
	_stage2.Reset();
}

double BiquadCascadeFilter::ProcessLeft(double in)
{
	return _stage2.ProcessLeft(_stage1.ProcessLeft(in));
}

double BiquadCascadeFilter::ProcessRight(double in)
{
	return _stage2.ProcessRight(_stage1.ProcessRight(in));
}
