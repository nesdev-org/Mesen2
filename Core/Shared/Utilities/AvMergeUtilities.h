#include "Shared/RenderedFrame.h"

template<class T>
struct VideoMergeResult
{
	RenderedFrame Frame;

	~VideoMergeResult()
	{
		delete[]((T*)Frame.FrameBuffer);
	}
};

class AvMergeUtilities
{
public:
	template<class T> static VideoMergeResult<T> MergeFrames(RenderedFrame& leftFrame, T* rightFrame)
	{
		//Merge both video frames into a single frame with twice the width
		uint32_t originalWidth = leftFrame.Width;
		RenderedFrame mergedFrame = leftFrame;

		mergedFrame.Width *= 2;

		T* in1 = (T*)leftFrame.FrameBuffer;
		T* in2 = rightFrame;

		T* out = new T[mergedFrame.Width * mergedFrame.Height * sizeof(T)];
		mergedFrame.FrameBuffer = out;

		for(uint32_t i = 0; i < mergedFrame.Height; i++) {
			memcpy(out, in1, originalWidth * sizeof(T));
			out += originalWidth;
			in1 += originalWidth;
			memcpy(out, in2, originalWidth * sizeof(T));
			out += originalWidth;
			in2 += originalWidth;
		}

		return VideoMergeResult<T> { mergedFrame };
	}

	static void MergeAudio(int16_t* inOutA, size_t& sampleCountA, int16_t* inB, size_t& sampleCountB)
	{
		//Merge both audio streams into one
		for(size_t i = 0; i < sampleCountA && i < sampleCountB; i++) {
			inOutA[i * 2] += inB[i * 2];
			inOutA[i * 2 + 1] += inB[i * 2 + 1];
		}

		//Move any extra samples back to the start of the buffer and update sample count accordingly
		if(sampleCountA < sampleCountB) {
			size_t samplesToCopy = sampleCountB - sampleCountA;
			memmove(inB, inB + sampleCountA * 2, samplesToCopy * 2 * sizeof(int16_t));
			sampleCountB = samplesToCopy;
		} else {
			sampleCountB = 0;
		}
	}
};