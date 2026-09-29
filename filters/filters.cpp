// filters.cpp : Defines the entry point for the application.
//

#include "filters.h"
#include "./analyzer/analyzer.h"
#pragma warning(push)
#pragma warning(disable:4267)
#pragma warning(disable:4305)
#pragma warning(disable:4309)
#pragma warning(disable:4267)
#pragma warning(disable:4244)
#include "AudioFile.h"
#pragma warning(pop)

namespace fs = std::filesystem;

void printHelp(char** argv) {
	fs::path fmain(argv[0]);
	std::cout << "Usage: " << fmain.filename() << " [options]" << '\n';
	std::cout << "\noptions can be:" << '\n';
	std::cout << "-help\t\tdisplay this help" << '\n';
	std::cout << "-sr {N}\t\tset sample rate to {N} Hz (can be " << MIN_SAMPLE_RATE << " to " << MAX_SAMPLE_RATE << ", default is " << DEF_SAMPLE_RATE << ")\n";
	std::cout << "-flthp {N}\tgenerate high-pass FIR filter of {N} points length (127+ recommended)" << '\n';
	std::cout << "-fltlp {N}\tgenerate low-pass FIR filter of {N} points length (127+ recommended)" << '\n';
	std::cout << "-fltfreq {N}\tset {N} Hz frequency for low/high-pass FIR filter (must be less than half sample rate)" << '\n';
	std::cout << "-fltwnd\t\tcalculate filter response with Kaiser windowing" << '\n';
	std::cout << "-fltinv\t\tcalculate filter response using inverse FFT transform" << '\n';
	std::cout << "-fft {N}\tset {N} minimal points in FFT transform used for calculating/drawing frequency response" << '\n';
	std::cout << "-outfolder {path}\tset folder (will be created if doesn't exist) for saving image/WAV file(s)" << '\n';
	std::cout << "-width {N}\tset width of output image file to {N} pixels (at least " << MIN_IMG_DIM << ")\n";
	std::cout << "\tIMPORTANT: if width isn't specified then full response will be drawn" << '\n';
	std::cout << "-height {N}\tset height of output image file to {N} pixels (at least " << MIN_IMG_DIM << ")\n";
	std::cout << "-dB\t\tuse vertical scale in decibels for drawing frequency response" << '\n';
	std::cout << "-range {dB}\tset vertical range (100 dB by default) for drawing in decibels" << '\n';
	std::cout << "-top {dB}\tset top of range (0 dB by default) for drawing in decibels" << '\n';
	std::cout << "-exp {N}\tdraw with exponential frequency scale, starting from {N} Hz (can be " << MIN_EXP_FREQ << " to " << MAX_EXP_FREQ << ")\n";
	std::cout << "\tIMPORTANT: width and starting frequency must be specified for exponential frequency scale" << '\n';
	std::cout << "-grid\t\tdraw grid on frequency/impulse response" << '\n';
	std::cout << "-bw\t\tdraw image in black and white (color by default)" << '\n';
	std::cout << "-gif\t\tsave to GIF (color only) instead of BMP" << '\n';
	std::cout << "-imp\t\tsave separate image with impulse response of generated filter" << '\n';
	std::cout << "-stepsize {K}\tset filter length step to {K} points (must be even) or frequency step to K Hz" << '\n';
	std::cout << "\tNOTE: step size can be negative for decreasing filter length or frequency" << '\n';
	std::cout << "-lensteps {N}\tgenerate {N} filters with length varying with {N} points step" << '\n';
	std::cout << "\tIMPORTANT: series of filters by length is generated if {N} > 1 and non-zero {K} (must be even)" << '\n';
	std::cout << "-freqsteps {M}\tgenerate {M} filters with frequency varying with {K} Hz step" << '\n';
	std::cout << "\tIMPORTANT: series of filters by frequency is generated if {M} > 1 and non-zero {K}" << '\n';
	std::cout << "-delay {D}\tsave series of filters to multi-frame GIF with delay in 1/100th sec, {D} must be > 0" << '\n';
	std::cout << "\tIMPORTANT: multi-frame GIF is saved only if image width is specified" << '\n';
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

	const auto [bOKlen, nFltLen] = io.getFltLen(input);
	if (!bOKlen)
		return retError();

	const auto [bOKfreq, nSR, nFltFreq, nMinFltFreq, nMaxFltFreq, nFFTsz] = io.getFltFreq(input);
	if (!bOKfreq)
		return retError();

	io.getImgOptions(input);

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

	const bool bKaiser = input.cmdOptionExists("-fltwnd"), bInv = input.cmdOptionExists("-fltinv"),
		bImp = input.cmdOptionExists("-imp");

	std::list<std::tuple<int, int, CFIRFilter>> lstFIR;
	{
		int nFLen = nFltLen, nFreq = nFltFreq;
		const bool bStepByFreq = io.StepByFreq();
		const int nSteps = bStepByFreq ? io.nFreqSteps : io.nLenSteps;
		for (int i = 0; i < nSteps; i++) {
			lstFIR.push_back({});
			auto& [flen, freq, fir] = lstFIR.back();
			if (io.LPF) {
				if (bInv)
					fir.InvLowHighPass<true>(bKaiser, nFLen, nFreq, nSR, nFFTsz);
				else
					fir.CalcLowPass(bKaiser, nFLen, nFreq, nSR, nFFTsz);
			}
			else {
				if (bInv)
					fir.InvLowHighPass<false>(bKaiser, nFLen, nFreq, nSR, nFFTsz);
				else
					fir.CalcHighPass(bKaiser, nFLen, nFreq, nSR, nFFTsz);
			}
			flen = nFLen;
			freq = nFreq;
			if (bStepByFreq) {
				nFreq += io.nStepSize;
				if (nFreq < nMinFltFreq || nFreq > nMaxFltFreq)
					break;
			}
			else {
				nFLen += io.nStepSize;
				if (nFLen < MIN_FIR_LENGTH || nFLen > MAX_FIR_LENGTH)
					break;
			}
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
		for (auto const& [nFLen, nFreq, fir] : lstFIR) {
			const auto [err, rMin, rMax] = an.saveImpImage(is, io, fir.GetFilter(), nFreq, bFirst, rngMin, rngMax);
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
	float rngMax = 0.0f;
	bool bFirst = true;
	for (auto const& [nFLen, nFreq, fir] : lstFIR) {
		const auto& flt = fir.GetFilter();
		const auto fsz = an.GetRespFIR(nFFTsz, (int)flt.size(), flt);
		is.SetFFTsz(fsz);
		const auto [err, rMax] = an.saveFRImage(is, io, nSR, nFreq, (bKaiser || io.dB) ? -1.0f : rngMax);
		if (err) {
			imgFailed(is.curFile(), err);
			break;
		}
		if (bFirst) {
			rngMax = rMax;
			bFirst = false;
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

	const auto& fir = std::get<2>(lstFIR.front());

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
