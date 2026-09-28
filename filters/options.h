#pragma once

#include <iostream>
#include "./external/inputparser.h"
#include "./filter/firfilter.h"

#define MIN_SAMPLE_RATE 1000
#define MAX_SAMPLE_RATE 100000
#define DEF_SAMPLE_RATE 48000
#define MIN_EXP_FREQ 10
#define MAX_EXP_FREQ 1000

#ifndef MIN_FFT_SIZE
#define MIN_FFT_SIZE 1024
#endif

#ifndef MIN_IMG_DIM
#define MIN_IMG_DIM 64
#endif

#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 480
#endif

#define MAX_FLT_STEPS 9999

struct imgOptions {
	bool dB = false,
		drawGrid = false,
		LPF = false,
		BW = false,
		GIF = false;
	int width = 0, height = 0, dBrange = 100, dBtop = 0, expHz = 0, nLenSteps = 0, nFreqSteps = 0, nStepSize = 0, nGIFdelay = 0;
	void CheckLenSteps() {
		if (nFreqSteps > 0) {
			if (std::abs(nStepSize) < 1)
				nFreqSteps = 0;
			else if (nFreqSteps < 2 || nFreqSteps > MAX_FLT_STEPS)
				nFreqSteps = 0;
		}
		if (0 != nLenSteps % 2)
			nLenSteps--;
		if (nLenSteps < 2 || nLenSteps > MAX_FLT_STEPS || std::abs(nStepSize) < 2) {
			nLenSteps = 0;
			if (nFreqSteps < 2)
				nStepSize = 0;
		}
	}
	bool StepByFreq() const {
		return (nFreqSteps > 1);
	}
	bool Full() const {
		return (width < MIN_IMG_DIM);
	}
	bool Exp() const {
		return (!Full() && expHz > 0);
	}
	int GetWidth() const {
		return (width - width % 2);
	}
	int GetHeight() const {
		return height >= MIN_IMG_DIM ? height : DEFAULT_HEIGHT;
	}
	bool MultiGIF() const {
		return (nGIFdelay > 0 && (nLenSteps > 1 || nFreqSteps > 1) && width > 0);
	}
	std::pair<bool, int> getFltLen(const InputParser& input);
	// bOK, nSR, nFltFreq, nMinFltFreq, nMaxFltFreq, nFFTsz
	std::tuple<bool, int, int, int, int, int> getFltFreq(const InputParser& input);
	void getImgOptions(const InputParser& input);
};