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

#define MAX_EQU_BANDS 100
#define MIN_GAIN_DB -48
#define MAX_GAIN_DB 24
#define MIN_OCT_STEP 0.1
#define MAX_OCT_STEP 2.0
#define DEFAULT_OCT_STEP 1
#define DEFAULT_BASE_FREQ 31.25

struct imgOptions {
	bool dB = false,
		drawGrid = false,
		LPF = false,
		BW = false,
		GIF = false;
	int width = 0, height = 0, dBrange = 100, dBtop = 0, expHz = 0, nLenSteps = 0, nFreqSteps = 0, nStepSize = 0, nGIFdelay = 0;
	double dBmaster = 0, octStep = DEFAULT_OCT_STEP, baseFreq = DEFAULT_BASE_FREQ;
	std::vector<double> dBequ, dBequ2, dBstep;
	bool IsEqu() const {
		return (dBequ.size() > 1);
	}
	void CheckLenSteps() {
		if (IsEqu()) {
			if (dBequ.size() != dBequ2.size() || nFreqSteps < 2)
				nFreqSteps = 0;
			if (nFreqSteps > 1) {
				dBstep.reserve(dBequ.size());
				const auto dk = 1.0 / double(nFreqSteps - 1);
				std::transform(dBequ.begin(), dBequ.end(), dBequ2.begin(), std::back_inserter(dBstep),
					[&dk](auto a, auto b) { return (b - a) * dk; });
				nLenSteps = 0;
			}
		}
		else if (nFreqSteps > 0) {
			if (std::abs(nStepSize) < 1)
				nFreqSteps = 0;
			else if (nFreqSteps < 2 || nFreqSteps > MAX_FLT_STEPS)
				nFreqSteps = 0;
			if (nFreqSteps > 1)
				nLenSteps = 0;
		}
		if (0 != nLenSteps % 2)
			nLenSteps--;
		if (nLenSteps < 2 || nLenSteps > MAX_FLT_STEPS || std::abs(nStepSize) < 2) {
			nLenSteps = 0;
			if (nFreqSteps < 2)
				nStepSize = 0;
		}
	}
	void equStep() {
		if (dBequ.size() == dBstep.size())
			std::transform(dBequ.begin(), dBequ.end(), dBstep.begin(), dBequ.begin(), std::plus<double>());
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