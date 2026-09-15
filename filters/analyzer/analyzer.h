#pragma once

#include <cmath>
#include <system_error>
#include <algorithm>
#include <filesystem>
#include <any>
#include "../external/fft4f.h" // based on https://www.kurims.kyoto-u.ac.jp/~ooura/fft.html
#include "../external/ezdib.h" // https://github.com/xiongyihui/ezdib

#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
#include <fmt/format.h>

#ifndef MIN_FFT_SIZE
#define MIN_FFT_SIZE 1024
#endif

#ifndef DB_INF
#define DB_INF -999
#endif

#ifndef MIN_IMG_DIM
#define MIN_IMG_DIM 64
#endif

#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 480
#endif

#define MAX_LEN_STEPS 9999

inline double Gain2dB(double g) {
	if (g <= 0) return DB_INF;
	double res = double(20.0 * log10(g));
	if (res < DB_INF) res = DB_INF;
	return res;
}

struct imgOptions {
	bool dB = false,
		drawGrid = false,
		LPF = false,
		BW = false,
		GIF = false;
	int width = 0, height = 0, dBrange = 100, dBtop = 0, expHz = 0, nLenSteps = 0, nStepSize = 0, nGIFdelay = 0;
	void CheckLenSteps() {
		if (0 != nLenSteps % 2)
			nLenSteps--;
		if (nLenSteps < 2 || nLenSteps > MAX_LEN_STEPS || nStepSize < 2) {
			nLenSteps = nStepSize = 0;
		}
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
		return (nLenSteps > 1 && width > 0 && nGIFdelay > 0);
	}
};

class CImgSaveHelper {
public:
	CImgSaveHelper() = delete;
	CImgSaveHelper(const imgOptions& io, const std::string& outfolder, const bool bWnd, const int nFltFreq, const int nFltLen)
		: m_io(io), m_outfolder(outfolder), m_bWnd(bWnd), m_nFltFreq(nFltFreq), m_nFltLen(nFltLen)
	{
		if (io.nLenSteps > 1)
			m_nIndex = 0;
	}
	~CImgSaveHelper();
	void SetFFTsz(const size_t nFFTsz) {
		m_nFFTsz = (int)nFFTsz;
	}
	std::pair<std::error_code, std::string> nextFilename();
	bool first() const { return m_nIndex < 2; }
	bool last() const { return (m_nIndex < 0 || m_nIndex >= m_io.nLenSteps); }
	auto& curFile() const { return m_curFile; }
	auto getWriter() { return &m_pGIFwriter; }
protected:
	std::error_code getFullPath(const std::string& outfolder, std::string& filename) const;
private:
	const imgOptions& m_io;
	const std::string m_outfolder;
	const bool m_bWnd;
	const int m_nFltFreq, m_nFltLen;
	int m_nIndex = -1, m_nFFTsz = 0;
	std::string m_curFile;
	std::any m_pGIFwriter;
};

class CAnalyzer {
public:
	CAnalyzer() {}
	size_t GetRespFIR(const int nFFTmin, const int nSamples, const std::vector<float>& data);
	std::error_code saveFRImage(CImgSaveHelper& is, const imgOptions& io, int nSampleRate, int nFreq);
	std::tuple<std::error_code, float, float> saveImpImage(CImgSaveHelper& is, const imgOptions& io, const std::vector<float>& imp,
		const bool bCalcRange = true, float rngMin = 0.0f, float rngMax = 0.0f);
protected:
	size_t getFFTsize(size_t nSamples, bool bUp) const;
	std::pair<int, double> CalcResp(const size_t fsz);
	std::tuple<bool, bool, int, int, HEZDIMAGE, HEZDFONT> initImage(const imgOptions& io, const std::vector<float>& data);
	std::error_code saveImage(CImgSaveHelper& is, const imgOptions& io, HEZDIMAGE hDib, HEZDFONT hFont, const int w, const int h);
	void saveGIF(CImgSaveHelper& is, const imgOptions& io, HEZDIMAGE hDib, const int w, const int h) const;
	template <bool dBdraw, typename T>
	void drawExp(const imgOptions& io, const std::vector<T>& draw, const float fmax, int nFreq, const int nSampleRate,
		const int w, const int h, const int dx, HEZDIMAGE hDib, HEZDFONT hFont);
	template <bool dBdraw, typename T>
	void drawLinear(const imgOptions& io, const std::vector<T>& draw, const float fmax, int nFreq, const int nSampleRate,
		const int w, const int h, const int dx, HEZDIMAGE hDib, HEZDFONT hFont);
	void drawFreq(HEZDIMAGE hDib, HEZDFONT hFont, int x, const int w, const int h, int nSampleRate, int nFreq, bool bGrid);
	int drawText(int nH, HEZDIMAGE hDib, HEZDFONT hFont, const char* pText, int x, int y, int x_col);
	void drawdBScale(HEZDIMAGE hDib, HEZDFONT hFont, int w, int h, int nRange, int nTop, bool bGrid);
	void drawAbsGrid(HEZDIMAGE hDib, HEZDFONT hFont, const float fmax, int w, int h, bool bGrid);
	void drawImpGrid(HEZDIMAGE hDib, HEZDFONT hFont, const float fmin, const float fmax, int w, int h, bool bGrid);
	void drawDotScale(HEZDIMAGE hDib, int w, int h, int x1, int x2, int y1, int y2, int nClr = -1) const;
	std::error_code getFullPath(const std::string& outfolder, std::string& filename) const;
private:
	TFFTF m_fft;
	// filter's impulse response values before FFT
	std::vector<float> m_data;
	// filter's frequency response values after FFT
	std::vector<float> m_fdata;
	// data for drawing with linear frequency scale
	std::vector<float> m_fdraw;
	std::vector<int> m_index;
	// data for drawing with exponential frequency scale
	std::vector<std::tuple<double, int, int, double>> m_expdraw;
	char m_str[256] = "";
	int m_nBpp = 24;
	const int m_clrBg = 0x888888, m_clrLine = 0xFFFF00, m_clrDot = 0xDDDDDD, m_clrFreq = 0x00FF00, m_clrDb = 0x00FFFF;
};
