#include "options.h"

constexpr int nFltFreqDef = 1000, nMinFltFreq = 10, nFFTszDef = 4096, nMaxDelay = 9999;

std::pair<bool, int> imgOptions::getFltLen(const InputParser& input) {
	bool bOK = true; int nFltLen = 0;
	auto checkFIRLen = [](const int nlen) {
		if (nlen < MIN_FIR_LENGTH || nlen > MAX_FIR_LENGTH) {
			std::cout << "Error: FIR filter length must be between " << MIN_FIR_LENGTH << " and " << MAX_FIR_LENGTH << '\n';
			return false;
		}
		return true;
	};
	if (input.cmdOptionExists("-fltlp")) {
		const auto& param = input.getThisOption();
		const auto nlen = atoi(param.c_str());
		if (!checkFIRLen(nlen))
			bOK = false;
		nFltLen = nlen;
		LPF = true;
	}
	else if (input.cmdOptionExists("-flthp")) {
		const auto& param = input.getThisOption();
		const auto nlen = atoi(param.c_str());
		if (!checkFIRLen(nlen))
			bOK = false;
		nFltLen = nlen;
		LPF = false;
	}
	if (nFltLen < MIN_FIR_LENGTH) {
		std::cout << "No options specified." << '\n';
		bOK = false;
	}
	else {
		if (input.cmdOptionExists("-stepsize")) {
			const auto& param = input.getThisOption();
			nStepSize = atoi(param.c_str());
		}
		if (input.cmdOptionExists("-lensteps")) {
			const auto& param = input.getThisOption();
			nLenSteps = atoi(param.c_str());
		}
		if (input.cmdOptionExists("-freqsteps")) {
			const auto& param = input.getThisOption();
			nFreqSteps = atoi(param.c_str());
		}
		CheckLenSteps();
	}
	return std::make_pair(bOK, nFltLen);
}

std::tuple<bool, int, int, int, int> imgOptions::getFltFreq(const InputParser& input) {
	bool bOK = true;
	int nFltFreq = nFltFreqDef, nFFTsz = nFFTszDef, nSR = DEF_SAMPLE_RATE, nMaxFltFreq = nSR * 499 / 1000;
	for (;;) {
		if (input.cmdOptionExists("-sr")) {
			const auto& param = input.getThisOption();
			nSR = atoi(param.c_str());
			if (nSR < MIN_SAMPLE_RATE || nSR > MAX_SAMPLE_RATE) {
				std::cout << "Error: sample rate must be between " << MIN_SAMPLE_RATE << " and " << MAX_SAMPLE_RATE << '\n';
				bOK = false; break;
			}
			nMaxFltFreq = nSR * 499 / 1000;
		}
		auto checkFIRFreq = [nMaxFltFreq](int freq) {
			if (freq < nMinFltFreq || freq > nMaxFltFreq) {
				std::cout << "Error: FIR filter frequency must be between " << nMinFltFreq << " and " << nMaxFltFreq << '\n';
				return false;
			}
			return true;
		};
		auto checkFFTsize = [](const int nfft) {
			bool bErr = (nfft < MIN_FFT_SIZE);
			if (!bErr) {
				int n = MIN_FFT_SIZE;
				constexpr int nMax = std::numeric_limits<int>::max() / 2;
				while (n < nfft && n < nMax)
					n *= 2;
				if (n != nfft)
					bErr = true;
			}
			if (bErr) {
				std::cout << "FFT size must be power of 2 and not less than " << MIN_FFT_SIZE << '\n';
				return false;
			}
			return true;
		};
		if (input.cmdOptionExists("-fltfreq")) {
			const auto& param = input.getThisOption();
			const auto freq = atoi(param.c_str());
			if (!checkFIRFreq(freq)) {
				bOK = false; break;
			}
			nFltFreq = freq;
		}
		if (input.cmdOptionExists("-fft")) {
			const auto& param = input.getThisOption();
			const auto nfft = atoi(param.c_str());
			if (!checkFFTsize(nfft)) {
				bOK = false; break;
			}
			nFFTsz = nfft;
		}
		break;
	}
	if (nLenSteps < 2)
		nLenSteps = 1;
	if (nFreqSteps < 2)
		nFreqSteps = 1;
	return std::make_tuple(bOK, nSR, nFltFreq, nMaxFltFreq, nFFTsz);
}

void imgOptions::getImgOptions(const InputParser& input) {
	if (input.cmdOptionExists("-width")) {
		const auto& param = input.getThisOption();
		const int nw = atoi(param.c_str());
		if (nw >= MIN_IMG_DIM)
			width = nw;
	}
	if (input.cmdOptionExists("-height")) {
		const auto& param = input.getThisOption();
		const int nh = atoi(param.c_str());
		if (nh >= MIN_IMG_DIM)
			height = nh;
	}
	dB = input.cmdOptionExists("-dB");
	if (input.cmdOptionExists("-range")) {
		const auto& param = input.getThisOption();
		const int range = atoi(param.c_str());
		if (range >= 10 && range <= 200)
			dBrange = range;
	}
	if (input.cmdOptionExists("-top")) {
		const auto& param = input.getThisOption();
		const int top = atoi(param.c_str());
		if (top > 0 && top <= 100)
			dBtop = top;
	}
	if (input.cmdOptionExists("-exp")) {
		const auto& param = input.getThisOption();
		const int hz = atoi(param.c_str());
		if (hz >= MIN_EXP_FREQ && hz <= MAX_EXP_FREQ)
			expHz = hz;
	}
	drawGrid = input.cmdOptionExists("-grid");
	BW = input.cmdOptionExists("-bw");
	GIF = input.cmdOptionExists("-gif");
	if (input.cmdOptionExists("-delay") && width > 0) {
		const auto& param = input.getThisOption();
		const int nd = atoi(param.c_str());
		if (nd > 0 && nd <= nMaxDelay)
			nGIFdelay = nd;
	}
}