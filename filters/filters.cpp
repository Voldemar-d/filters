// filters.cpp : Defines the entry point for the application.
//

#include "filters.h"
#include "./filter/firfilter.h"
#include "./analyzer/analyzer.h"
#pragma warning(push)
#pragma warning(disable:4267)
#pragma warning(disable:4305)
#pragma warning(disable:4309)
#pragma warning(disable:4267)
#pragma warning(disable:4244)
#include "AudioFile.h"
#pragma warning(pop)

constexpr int nDefSampleRate = 48000, nMinSampleRate = 1000, nMaxSampleRate = 100000,
nFIRfreqDef = 1000, nMinFIRfreq = 10, nMinExpFreq = 10, nMaxExpFreq = 1000, nFFTszDef = 4096, nMaxDelay = 9999;

namespace fs = std::filesystem;

void printHelp(char** argv) {
	fs::path fmain(argv[0]);
	std::cout << "Usage: " << fmain.filename() << " [options]" << '\n';
	std::cout << "\noptions can be:" << '\n';
	std::cout << "-help\t\tdisplay this help" << '\n';
	std::cout << "-sr {N}\t\tset sample rate to {N} Hz (can be " << nMinSampleRate << " to " << nMaxSampleRate << ", default is " << nDefSampleRate << ")\n";
	std::cout << "-flthp {N}\tgenerate high-pass FIR filter of {N} points length (127+ recommended)" << '\n';
	std::cout << "-fltlp {N}\tgenerate low-pass FIR filter of {N} points length (127+ recommended)" << '\n';
	std::cout << "-fltfreq {N}\tset {N} Hz frequency for low/high-pass FIR filter (must be less than half sample rate)" << '\n';
	std::cout << "-fltwnd\t\tcalculate filter response with Kaiser windowing" << '\n';
	std::cout << "-fft {N}\tset {N} minimal points in FFT transform used for drawing frequency response" << '\n';
	std::cout << "-outfolder\tset output folder (will be created it doesn't exist) for saving image file(s)" << '\n';
	std::cout << "-width {N}\tset width of output image file to {N} pixels (at least " << MIN_IMG_DIM << ")\n";
	std::cout << "\tIMPORTANT: if width isn't specified then full response will be drawn" << '\n';
	std::cout << "-height {N}\tset height of output image file to {N} pixels (at least " << MIN_IMG_DIM << ")\n";
	std::cout << "-dB\t\tuse vertical scale in decibels for drawing frequency response" << '\n';
	std::cout << "-range {dB}\tset vertical range (100 dB by default) for drawing in decibels" << '\n';
	std::cout << "-top {dB}\tset top of range (0 dB by default) for drawing in decibels" << '\n';
	std::cout << "-exp {N}\tdraw with exponential frequency scale, starting from {N} Hz (can be " << nMinExpFreq << " to " << nMaxExpFreq << ")\n";
	std::cout << "\tIMPORTANT: width and starting frequency must be specified for exponential frequency scale" << '\n';
	std::cout << "-grid\t\tdraw grid on frequency/impulse response" << '\n';
	std::cout << "-bw\t\tdraw image in black and white (color by default)" << '\n';
	std::cout << "-gif\t\tsave to GIF (color only) instead of BMP" << '\n';
	std::cout << "-imp\t\tsave separate image with impulse response of generated filter" << '\n';
	std::cout << "-stepsize {K}\tset step of filter length to K points (must be even)" << '\n';
	std::cout << "-lensteps {N}\tgenerate N filters with length increasing with step of K points" << '\n';
	std::cout << "\tIMPORTANT: series of filters is generated only if N > 1 and K > 1 (must be even)" << '\n';
	std::cout << "-delay {N}\tsave series of filters to multi-frame GIF with delay in 1/100th sec (must be > 0)" << '\n';
	std::cout << "\tIMPORTANT: multi-frame GIF is saved only if width is specified" << '\n';
	std::cout << "-wav {infile}\tload WAV file, process with generated filter and save result to output folder" << '\n';
}

std::string getFullPath(std::string_view folder) {
	fs::path path(folder);
	if (path.has_root_path()) // already full path
		return std::string(folder);
	auto curpath = fs::absolute(path);
	return curpath.string();
}

int main(int argc, char* argv[])
{
	std::vector<std::string> all_args;
	if (argc > 1)
		all_args.assign(argv + 1, argv + argc);
	InputParser input(all_args);

	if (input.cmdOptionExists("-help")) {
		printHelp(argv);
		return 0;
	}

	auto retError = []() {
		std::cout << "Use -help for more information." << '\n';
		return -1;
	};

	imgOptions io;

	auto getFltLen = [&input, &io]() {
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
			io.LPF = true;
		}
		else if (input.cmdOptionExists("-flthp")) {
			const auto& param = input.getThisOption();
			const auto nlen = atoi(param.c_str());
			if (!checkFIRLen(nlen))
				bOK = false;
			nFltLen = nlen;
			io.LPF = false;
		}
		if (nFltLen < MIN_FIR_LENGTH) {
			std::cout << "No options specified." << '\n';
			bOK = false;
		}
		else {
			if (input.cmdOptionExists("-stepsize")) {
				const auto& param = input.getThisOption();
				io.nStepSize = atoi(param.c_str());
			}
			if (input.cmdOptionExists("-lensteps")) {
				const auto& param = input.getThisOption();
				io.nLenSteps = atoi(param.c_str());
			}
			io.CheckLenSteps();
		}
		return std::make_pair(bOK, nFltLen);
	};
	const auto [bOKlen, nFltLen] = getFltLen();
	if (!bOKlen)
		return retError();

	auto getFltFreq = [&input]() {
		bool bOK = true;
		int nFltFreq = nFIRfreqDef, nFFTsz = nFFTszDef, nSR = nDefSampleRate, nMaxFIRfreq = nSR * 499 / 1000;
		for (;;) {
			if (input.cmdOptionExists("-sr")) {
				const auto& param = input.getThisOption();
				nSR = atoi(param.c_str());
				if (nSR < nMinSampleRate || nSR > nMaxSampleRate) {
					std::cout << "Error: sample rate must be between " << nMinSampleRate << " and " << nMaxSampleRate << '\n';
					bOK = false; break;
				}
				nMaxFIRfreq = nSR * 499 / 1000;
			}
			auto checkFIRFreq = [nMaxFIRfreq](int freq) {
				if (freq < nMinFIRfreq || freq > nMaxFIRfreq) {
					std::cout << "Error: FIR filter frequency must be between " << nMinFIRfreq << " and " << nMaxFIRfreq << '\n';
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
		return std::make_tuple(bOK, nSR, nFltFreq, nFFTsz);
	};
	const auto [bOKfreq, nSR, nFltFreq, nFFTsz] = getFltFreq();
	if (!bOKfreq)
		return retError();

	if (input.cmdOptionExists("-width")) {
		const auto& param = input.getThisOption();
		const int width = atoi(param.c_str());
		if (width >= MIN_IMG_DIM)
			io.width = width;
	}
	if (input.cmdOptionExists("-height")) {
		const auto& param = input.getThisOption();
		const int height = atoi(param.c_str());
		if (height >= MIN_IMG_DIM)
			io.height = height;
	}
	if (input.cmdOptionExists("-dB"))
		io.dB = true;
	if (input.cmdOptionExists("-range")) {
		const auto& param = input.getThisOption();
		const int range = atoi(param.c_str());
		if (range >= 10 && range <= 200)
			io.dBrange = range;
	}
	if (input.cmdOptionExists("-top")) {
		const auto& param = input.getThisOption();
		const int top = atoi(param.c_str());
		if (top > 0 && top <= 100)
			io.dBtop = top;
	}
	if (input.cmdOptionExists("-exp")) {
		const auto& param = input.getThisOption();
		const int hz = atoi(param.c_str());
		if (hz >= nMinExpFreq && hz <= nMaxExpFreq)
			io.expHz = hz;
	}
	if (input.cmdOptionExists("-grid"))
		io.drawGrid = true;
	if (input.cmdOptionExists("-bw"))
		io.BW = true;
	if (input.cmdOptionExists("-gif"))
		io.GIF = true;
	if (input.cmdOptionExists("-delay") && io.width > 0) {
		const auto& param = input.getThisOption();
		const int ms = atoi(param.c_str());
		if (ms > 0 && ms <= nMaxDelay)
			io.nGIFdelay = ms;
	}

	std::string outfolder;

	if (input.cmdOptionExists("-outfolder")) {
		const auto& param = input.getThisOption();
		outfolder = getFullPath(param);
		if (!fs::exists(outfolder) && !fs::create_directory(outfolder)) {
			std::cout << "Failed to create output folder: " << outfolder << '\n';
			return retError();
		}
	}
	if (outfolder.empty()) {
		std::cout << "Output folder not specified." << outfolder << '\n';
		return retError();
	}

	const bool bKaiser = input.cmdOptionExists("-fltwnd"),
		bImp = input.cmdOptionExists("-imp");

	std::list<std::pair<int, CFIRFilter>> lstFIR;
	if (io.nLenSteps < 2)
		io.nLenSteps = 1;
	{
		int nFLen = nFltLen;
		for (int i = 0; i < io.nLenSteps; i++) {
			lstFIR.push_back({});
			lstFIR.back().first = nFLen;
			auto& fir = lstFIR.back().second;
			if (io.LPF)
				fir.CalcLowPass(bKaiser, nFLen, nFltFreq, nSR, nFFTsz);
			else
				fir.CalcHighPass(bKaiser, nFLen, nFltFreq, nSR, nFFTsz);
			nFLen += io.nStepSize;
		}
	}
	CAnalyzer an; bool bOK = true;
	auto imgFailed = [&bOK](const auto& fname, const auto err) {
		std::cout << "Failed to save output image file: " << fname << '\n';
		std::cout << "Error " << err << '\n';
		bOK = false;
	};
	if (bImp) { // save impulse response image(s)
		CImgSaveHelper is(io, outfolder, bKaiser, nFltFreq, nFltLen);
		float rngMin = 0, rngMax = 0;
		bool bFirst = true;
		for (auto const& [nFLen, fir] : lstFIR) {
			const auto [err, rMin, rMax] = an.saveImpImage(is, io, fir.GetFilter(), bFirst, rngMin, rngMax);
			if (err) {
				imgFailed(is.curFile(), err);
				break;
			}
			if (bFirst) {
				rngMin = rMin; rngMax = rMax;
				bFirst = false;
			}
		}
	}
	if (!bOK)
		return retError();
	// save frequency response image(s)
	CImgSaveHelper is(io, outfolder, bKaiser, nFltFreq, nFltLen);
	for (auto const& [nFLen, fir] : lstFIR) {
		const auto& flt = fir.GetFilter();
		const auto fsz = an.GetRespFIR(nFFTsz, (int)flt.size(), flt);
		is.SetFFTsz(fsz);
		const auto err = an.saveFRImage(is, io, nSR, nFltFreq);
		if (err) {
			imgFailed(is.curFile(), err);
			break;
		}
	}
	if (!bOK)
		return retError();

	std::string inwav, outwav;

	if (input.cmdOptionExists("-wav")) {
		const auto& param = input.getThisOption();
		fs::path inpath(param);
		if (!inpath.has_filename()) {
			std::cout << "Invalid path to input WAV file: " << inpath << '\n';
			return retError();
		}
		fs::path outpath(outfolder);
		outpath.append(inpath.filename().c_str());
		if (inpath == outpath) {
			std::cout << "Can't save WAV file to the same folder: " << outfolder << '\n';
			return retError();
		}
		inwav = inpath.string();
		outwav = outpath.string();
	}

	if (inwav.empty())
		return 0;

	AudioFile<float> audioFile;
	if (!audioFile.load(inwav)) {
		std::cout << "Failed to load input WAV file: " << inwav << '\n';
		return retError();
	}
	std::cout << "Loaded input WAV file: " << inwav << '\n';
	audioFile.printSummary();

	const auto nASR = audioFile.getSampleRate();
	const int numChannels = audioFile.getNumChannels(), numSamples = audioFile.getNumSamplesPerChannel();

	const auto& fir = lstFIR.front().second;

	std::vector<CFIRFilter> lstAFIR; lstAFIR.reserve(numChannels);
	for (int i = 0; i < numChannels; i++)
		lstAFIR.emplace_back(fir);

	float x;
	for (int i = 0; i < numSamples; i++) {
		for (int channel = 0; channel < numChannels; channel++) {
			auto& sample = audioFile.samples[channel][i];
			x = lstAFIR[channel].ApplySym(sample);
			sample = x;
		}
	}

	std::cout << "Processed " << numSamples << " in " << numChannels << " channels" << '\n';

	if (!audioFile.save(outwav, AudioFileFormat::Wave))
	{
		std::cout << "Failed to save output WAV file: " << outwav << '\n';
		return retError();
	}

	std::cout << "Saved output WAV file: " << outwav << '\n';

	return 0;
}
