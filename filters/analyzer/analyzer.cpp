#include "analyzer.h"

#pragma warning(push)
#pragma warning(disable:4334)
#include "../external/gif.h" // https://github.com/charlietangora/gif-h.git
#pragma warning(pop)

namespace fs = std::filesystem;

CImgSaveHelper::~CImgSaveHelper() {
	if (m_pGIFwriter.has_value()) {
		try {
			auto wr_ptr = std::any_cast<std::shared_ptr<GifWriter>>(m_pGIFwriter);
			wr_ptr.reset();
		}
		catch (const std::bad_any_cast&) {
		}
	}
}

std::pair<std::error_code, std::string> CImgSaveHelper::nextFilename()
{
	const bool bIndex = (m_nIndex > -1), bMultiGIF = m_io.GIF && m_io.MultiGIF();
	const int nFltLen = m_nFltLen + (bIndex && !bMultiGIF ? m_nIndex * m_io.nStepSize : 0);
	auto fname = fmt::format("{:s}_", m_io.LPF ? "LowPass" : "HighPass");
	if (!bIndex)
		fname += fmt::format("{:d}pt_{:d}hz", nFltLen, m_nFltFreq);
	else
		fname += fmt::format("{:d}hz", m_nFltFreq);
	if (m_nFFTsz > 0) {
		fname += fmt::format("_fft{:d}", m_nFFTsz);
		fname += m_io.Exp() ? "_exp" : "_lin";
		fname += m_io.dB ? "_dB" : "_abs";
	}
	else
		fname += "_imp";
	fname += m_bWnd ? "_wnd" : "_nownd";
	if (m_io.BW)
		fname += "_BW";
	if (bIndex) {
		if (bMultiGIF)
			fname += fmt::format("_{:d}-{:d}pt_{:d}", nFltLen, nFltLen + (m_io.nLenSteps - 1) * m_io.nStepSize, m_io.nLenSteps);
		else
			fname += fmt::format("_{:d}pt_{:04d}", nFltLen, m_nIndex + 1);
	}
	fname += m_io.GIF ? ".gif" : ".bmp";
	const auto err = getFullPath(m_outfolder, fname);
	m_curFile = fname;
	if (!err && bIndex)
		m_nIndex++;
	return { err, fname };
}

std::error_code CImgSaveHelper::getFullPath(const std::string& outfolder, std::string& filename) const
{
	if (!fs::exists(outfolder)) return std::make_error_code(std::errc::no_such_file_or_directory);
	fs::path path(outfolder);
	path.append(filename);
	filename = path.string();
	return std::make_error_code(std::errc());
}

size_t CAnalyzer::getFFTsize(size_t nSamples, bool bUp) const {
	size_t fsz = 2;
	while (fsz < nSamples)
		fsz <<= 1;
	if (fsz > nSamples && !bUp)
		fsz >>= 1;
	return fsz;
}

size_t CAnalyzer::GetRespFIR(const int nFFTmin, const int nSamples, const std::vector<float>& data) {
	const auto fsz = std::max(getFFTsize(nSamples, true), size_t(std::max(nFFTmin, MIN_FFT_SIZE)));
	m_data.clear();
	m_data.resize(fsz * 2);
	std::fill(m_data.begin(), m_data.end(), 0.0f);
	const auto n = std::min((size_t)nSamples, fsz);
	for (size_t i = 0; i < n; i++)
		m_data[i * 2] = data[i];
	m_fft.SetSize((int)fsz);
	m_fft.CDFT(m_data.data());
	CalcResp((int)fsz);
	return fsz;
}

std::pair<int, double> CAnalyzer::CalcResp(const size_t fsz) {
	const auto fdata = m_data.data();
	const int nf = int(fsz / 2);
	double re, im, val, vmax = 0;
	int i, imax = -1;
	m_fdata.resize(nf, 0);
	for (i = 0; i < nf; i++) {
		re = fdata[i * 2];
		im = fdata[i * 2 + 1];
		val = sqrt(re * re + im * im);
		m_fdata[i] = (float)val;
		if (vmax < val) {
			imax = i;
			vmax = val;
		}
	}
	return { imax, vmax };
}

std::tuple<bool, bool, int, int, HEZDIMAGE, HEZDFONT> CAnalyzer::initImage(const imgOptions& io, const std::vector<float>& data) {
	bool bOK = !data.empty(), bFull = io.Full();
	int w = 0, h = 0;
	HEZDIMAGE hDib = nullptr; HEZDFONT hFont = nullptr;
	if (bOK) {
		if (io.GIF)
			m_nBpp = 32;
		else
			m_nBpp = io.BW ? 1 : 24;
		w = bFull ? (int)data.size() : io.GetWidth();
		h = io.GetHeight();

		// Create an image
		hDib = ezd_create(w, h, m_nBpp, 0);
		// Load font
		hFont = ezd_load_font(EZD_FONT_TYPE_MEDIUM, 0, 0);

		// Fill in the background with background color
		ezd_fill(hDib, (1 == m_nBpp) ? 0 : m_clrBg);
	}
	return { bOK, bFull, w, h, hDib, hFont };
}

std::error_code CAnalyzer::saveImage(CImgSaveHelper& is, const imgOptions& io, HEZDIMAGE hDib, HEZDFONT hFont, const int w, const int h)
{
	const auto [err, filename] = is.nextFilename();
	if (!err) {
		if (io.GIF) // Save image to GIF
			saveGIF(is, io, hDib, w, h);
		else // Save image to BMP
			ezd_save(hDib, filename.c_str());

		// Free the memory
		ezd_destroy_font(hFont);

		// Free resources
		ezd_destroy(hDib);

		if (io.MultiGIF() && !is.last())
			return err;
		fmt::print("Saved image {:d} x {:d} pixels to file: {:s}\n", w, h, filename);
	}
	return err;
}

std::error_code CAnalyzer::saveFRImage(CImgSaveHelper& is, const imgOptions& io, int nSampleRate, int nFreq)
{
	auto err = std::make_error_code(std::errc::permission_denied);
	const auto [bOK, bFull, w, h, hDib, hFont] = initImage(io, m_fdata);
	if (bOK) {
		const int dx = 50;
		const auto& src = m_fdata;
		int i, j, nHz, n = (int)src.size();
		const auto sdata = src.begin();
		const double nd = double(n);
		float dmin = 0, dmax = 0, fmax = 0.0f;
		// current graph direction
		bool bUp = true;

		if (io.Exp()) { // exponential frequency scale
			const double dfmin = double(io.expHz),
				dfmax = 0.5 * double(nSampleRate),
				wd1 = 1.0 / double(w),
				d1f = 1.0 / dfmax,
				klog = log(dfmax / dfmin);
			m_expdraw.clear();
			int jprev = 0, jlast = 0;
			m_expdraw.emplace_back(0, jprev, 0, 0);
			double dfreq;
			for (i = 1; i < w; i++) {
				dfreq = dfmin * exp(klog * double(i) * wd1);
				j = int(nd * dfreq * d1f + 0.5);
				if (j > jprev) {
					// for every pixel: frequency (Hz), index in data array, x coordinate, frequency responce value in point (to be determined later)
					m_expdraw.emplace_back(dfreq, j, i, 0);
					jprev = j;
				}
			}
			n = (int)m_expdraw.size();
			if (0 == n % 2)
				n--;
			for (j = 0; j < n - 1; j += 2) {
				const auto [min1, max1] = std::minmax_element(sdata + std::get<1>(m_expdraw[j]), sdata + std::get<1>(m_expdraw[j + 1]));
				const auto [min2, max2] = std::minmax_element(sdata + std::get<1>(m_expdraw[j + 1]), sdata + std::get<1>(m_expdraw[j + 2]));
				dmin = std::min(*min1, *min2);
				dmax = std::max(*max2, *max2);
				if (*min2 < *min1 && *max2 < *min1)
					bUp = false;
				else
					bUp = true;
				std::get<3>(m_expdraw[j]) = bUp ? dmin : dmax;
				jlast = j + 1;
				std::get<3>(m_expdraw[jlast]) = bUp ? dmax : dmin;
				fmax = std::max(fmax, dmax);
			}
			if (jlast > 0) {
				n = (int)m_expdraw.size();
				for (j = jlast + 1; j < n; j++)
					std::get<3>(m_expdraw[j]) = bUp ? dmax : dmin;
			}
			if (io.dB) // draw in decibels
				drawExp<true>(io, m_expdraw, fmax, nFreq, nSampleRate, w, h, dx, hDib, hFont);
			else // draw absolute values
				drawExp<false>(io, m_expdraw, fmax, nFreq, nSampleRate, w, h, dx, hDib, hFont);
			for (i = 0; i < w; i++) {
				if (0 == i)
					drawFreq(hDib, hFont, 0, w, h, nSampleRate, 0, io.drawGrid);
				else if (0 == i % dx) {
					nHz = int(dfmin * exp(klog * double(i) * wd1) + 0.5);
					drawFreq(hDib, hFont, i, w, h, nSampleRate, nHz, io.drawGrid);
				}
			}
			drawFreq(hDib, hFont, w, w, h, nSampleRate, 0, io.drawGrid);
		}
		else { // linear frequency scale
			const auto& draw = bFull ? m_fdata : m_fdraw;
			if (bFull) { // full data
				for (auto const& val : draw)
					fmax = std::max(fmax, val);
			}
			else { // specified width
				m_fdraw.resize(w);
				m_index.clear();
				m_index.reserve(w + 1);
				const double dw = 1.0 / double(w);
				for (i = 0; i < w; i++) {
					j = int(double(i) * nd * dw + 0.5);
					m_index.push_back(std::min(j, n - 1));
				}
				m_index.push_back(n - 1);
				n = (int)m_index.size();
				for (j = 0; j < n - 1; j += 2) {
					const auto [min1, max1] = std::minmax_element(sdata + m_index[j], sdata + m_index[j + 1]);
					const auto [min2, max2] = std::minmax_element(sdata + m_index[j + 1], sdata + m_index[j + 2]);
					dmin = std::min(*min1, *min2);
					dmax = std::max(*max1, *max2);
					if (*min2 < *min1 && *max2 < *min1)
						bUp = false;
					else
						bUp = true;
					m_fdraw[j] = bUp ? dmin : dmax;
					m_fdraw[j + 1] = bUp ? dmax : dmin;
					fmax = std::max(fmax, dmax);
				}
			}
			if (io.dB) // draw in decibels
				drawLinear<true>(io, draw, fmax, nFreq, nSampleRate, w, h, dx, hDib, hFont);
			else // draw absolute values
				drawLinear<false>(io, draw, fmax, nFreq, nSampleRate, w, h, dx, hDib, hFont);
			drawFreq(hDib, hFont, w, w, h, nSampleRate, 0, io.drawGrid);
		}

		err = saveImage(is, io, hDib, hFont, w, h);
	}
	return err;
}

void CAnalyzer::saveGIF(CImgSaveHelper& is, const imgOptions& io, HEZDIMAGE hDib, const int w, const int h) const
{
	auto refWriter = is.getWriter();
	std::shared_ptr<GifWriter> pgw;
	if (refWriter->has_value()) {
		try {
			auto wr_ptr = std::any_cast<std::shared_ptr<GifWriter>>(*refWriter);
			pgw = wr_ptr;
		}
		catch (const std::bad_any_cast&) {
		}
	}
	if (nullptr == pgw) {
		pgw = std::make_shared<GifWriter>();
		*refWriter = pgw;
	}
	if (nullptr == pgw) return;
	if (is.first())
		GifBegin(pgw.get(), is.curFile().c_str(), w, h, io.nGIFdelay);
	const auto pData = ((uint8_t*)hDib) + ezd_header_size() - 4;
	// vertical flip the image and convert BGRA => RGBA
	const size_t szLine = w * 4;
	std::vector<uint8_t> line(szLine);
	auto pSrc = pData, pDst = pSrc + (h - 1) * szLine, pFrom = pSrc, pTo = pDst;
	uint8_t Bf, Gf, Rf, Bt, Gt, Rt;
	for (int i = 0; i < h / 2; i++) {
		pFrom = pSrc; pTo = pDst;
		for (int j = 0; j < w; j++) {
			Bf = pFrom[0];
			Gf = pFrom[1];
			Rf = pFrom[2];
			Bt = pTo[0];
			Gt = pTo[1];
			Rt = pTo[2];
			pFrom[0] = Rt;
			pFrom[1] = Gt;
			pFrom[2] = Bt;
			pTo[0] = Rf;
			pTo[1] = Gf;
			pTo[2] = Bf;
			pFrom += 4; pTo += 4;
		}
		pSrc += szLine;
		pDst -= szLine;
	}
	GifWriteFrame(pgw.get(), pData, w, h, io.nGIFdelay);
	if (is.last() || !io.MultiGIF()) {
		GifEnd(pgw.get());
		pgw.reset();
		refWriter->reset();
	}
}

template <bool dBdraw, typename T>
void CAnalyzer::drawExp(const imgOptions& io, const std::vector<T>& draw, const float fmax, int nFreq, const int nSampleRate,
	const int w, const int h, const int dx, HEZDIMAGE hDib, HEZDFONT hFont)
{
	const auto hd = fabs(double(h));
	int x1 = 0, y, y1, nHz;
	const double df = hd / double(io.dBrange), dt = double(io.dBtop);
	double dB;
	if constexpr (dBdraw) { // draw in decibels
		dB = Gain2dB(m_fdata.front()) - dt;
		y1 = h + int(df * dB);
	}
	else
		y1 = int(hd * m_fdata.front() / fmax);
	// for every pixel: frequency (Hz), index in data array, x coordinate, frequency responce value in point
	for (auto const [hz, vi, x, vr] : m_expdraw) {
		if constexpr (dBdraw) {
			dB = Gain2dB(vr) - dt;
			y = h + int(df * dB);
			if (y < 0)
				y = 0;
		}
		else
			y = int(hd * vr / fmax);
		if (x > 0)
			ezd_line(hDib, x1, y1, x, y, m_clrLine);
		if (x == w - 1)
			x1 = x;
		nHz = int(hz + 0.5);
		if (nHz >= nFreq) { // draw filter's cutoff frequency
			drawFreq(hDib, hFont, -x, w, h, nSampleRate, nFreq, io.drawGrid);
			nFreq = nSampleRate;
		}
		x1 = x; y1 = y;
	}
	if constexpr (dBdraw)
		drawdBScale(hDib, hFont, w, h, io.dBrange, io.dBtop, io.drawGrid);
	else
		drawAbsGrid(hDib, hFont, fmax, w, h, io.drawGrid);
}

template <bool dBdraw, typename T>
void CAnalyzer::drawLinear(const imgOptions& io, const std::vector<T>& draw, const float fmax, int nFreq, const int nSampleRate,
	const int w, const int h, const int dx, HEZDIMAGE hDib, HEZDFONT hFont)
{
	const auto hd = fabs(double(h)), wd = double(w), wd1 = 1.0 / wd;
	const double dnfw = 0.5 * double(nSampleRate) * wd1,
		df = hd / double(io.dBrange), dt = double(io.dBtop);
	double dB;
	int x = 0, x1 = 0, y, y1, nHz;
	if constexpr (dBdraw) { // draw in decibels
		dB = Gain2dB(m_fdata.front()) - dt;
		y1 = h + int(df * dB);
	}
	else
		y1 = int(hd * m_fdata.front());
	for (auto const& val : draw) {
		if constexpr (dBdraw) {
			dB = Gain2dB(val) - dt;
			y = h + int(df * dB);
			if (y < 0)
				y = 0;
		}
		else
			y = int(hd * val / fmax);
		if (x > 0)
			ezd_line(hDib, x1, y1, x, y, m_clrLine);
		if (0 == x % dx)
			drawFreq(hDib, hFont, x, w, h, nSampleRate, 0, io.drawGrid);
		nHz = int(double(x) * dnfw + 0.5);
		if (nHz >= nFreq) { // draw filter's cutoff frequency
			drawFreq(hDib, hFont, -x, w, h, nSampleRate, nFreq, io.drawGrid);
			nFreq = nSampleRate;
		}
		x1 = x; y1 = y;
		x++;
	}
	if constexpr (dBdraw)
		drawdBScale(hDib, hFont, w, h, io.dBrange, io.dBtop, io.drawGrid);
	else
		drawAbsGrid(hDib, hFont, fmax, w, h, io.drawGrid);
}

void CAnalyzer::drawFreq(HEZDIMAGE hDib, HEZDFONT hFont, int x, const int w, const int h, int nSampleRate, int nFreq, bool bGrid) {
	int y = 8, x1 = x - 1;
	if (bGrid)
		drawDotScale(hDib, w, h, x, x, 0, h);
	if (x >= w)
		ezd_line(hDib, x1, 0, x1, y, m_clrFreq);
	else if (x >= 0)
		ezd_line(hDib, x, 0, x, y, m_clrFreq);

	if (w > 0) {
		nSampleRate >>= 1;
		fmt::format_to(m_str, "{:d}{:c}", (nFreq < 1) ? int(double(x) * double(nSampleRate) / double(w) + 0.5) : nFreq, '\0');
		int nW = 0, nH = 0;
		ezd_text_size(hFont, m_str, 0, &nW, &nH);
		if (nW > 0 && nH > 0) {
			if (x < 0) { // draw filter's cutoff frequency
				x = -x + 3;
				y = h - nH - 4;
				if (m_nBpp > 1) y = y - nH + 2;
				drawDotScale(hDib, w, h, x, x, 0, h, m_clrFreq);
			}
			else if (0 == x)
				x = 2;
			else
				x -= nW / 2;
			if (x + nW >= w) {
				x = w - nW - 2;
				y = 0;
			}
			drawText(nH, hDib, hFont, m_str, x, y, m_clrFreq);
		}
	}
}

int CAnalyzer::drawText(int nH, HEZDIMAGE hDib, HEZDFONT hFont, const char* pText, int x, int y, int clr) {
	if (1 == m_nBpp)
		return ezd_text(hDib, hFont, pText, 0, x, y, 0xFFFFFF);
	return ezd_text(hDib, hFont, pText, 0, x, y + nH - 1, clr);
}

void CAnalyzer::drawdBScale(HEZDIMAGE hDib, HEZDFONT hFont, int w, int h, int nRange, int nTop, bool bGrid)
{
	int nW = 0, nH = 0, ndB = nTop;
	fmt::format_to(m_str, "{:d}{:c}", ndB, '\0');
	ezd_text_size(hFont, m_str, 0, &nW, &nH);
	int y = h - nH - 2, yt = y - 1;
	drawText(nH, hDib, hFont, m_str, 2, (1 == m_nBpp) ? y : y - nH + 2, m_clrDb);
	y = 0;
	ndB = nTop - nRange;
	fmt::format_to(m_str, "{:d}{:c}", ndB, '\0');
	drawText(nH, hDib, hFont, m_str, 0, y, m_clrDb);
	const int dy = nH * 2, dh = nH / 2, yz = h - h * nTop / nRange;
	if (yz < yt) {
		drawDotScale(hDib, w, h, 0, w, yz, yz, m_clrDb);
		ndB = 0;
		fmt::format_to(m_str, "-{:d}-{:c}", ndB, '\0');
		drawText(nH, hDib, hFont, m_str, nW + 2, yz - dh - 1, m_clrDb);
	}
	while (y < yt) {
		y += dy;
		if (y != yz)
			drawDotScale(hDib, w, h, 0, w, y, y);
		if (y + nH < yt) {
			ndB = nTop - nRange * (h - y) / h;
			fmt::format_to(m_str, "{:d}{:c}", ndB, '\0');
			drawText(nH, hDib, hFont, m_str, 2, y - dh, m_clrDb);
		}
	}
}

void CAnalyzer::drawAbsGrid(HEZDIMAGE hDib, HEZDFONT hFont, const float fmax, int w, int h, bool bGrid) {
	if (!bGrid) return;
	int n = int(fmax * 10.0 + 0.5);
	auto drawVal = [this, &hDib, &hFont](const int y, double v) {
		int nW = 0, nH = 0;
		fmt::format_to(m_str, "{:.1f}{:c}", v, '\0');
		ezd_text_size(hFont, m_str, 0, &nW, &nH);
		drawText(nH, hDib, hFont, m_str, 2, y, m_clrDb);
	};
	if (n < 11) {
		n = 10;
		for (int j = 0; j < n; j++) {
			const int y = int(double(h * j) * 0.1 + 0.5);
			drawDotScale(hDib, w, h, 0, w, y, y, m_clrDot);
			drawVal(y, double(j) * 0.1);
		}
		drawDotScale(hDib, w, h, 0, w, h - 1, h - 1, m_clrDot);
	}
	else { // fmax > 1.0
		const auto k = 1.0 / (10.0 * fmax);
		for (int j = 0; j <= n; j++) {
			const int y = int(double(h * j) * k + 0.5);
			drawDotScale(hDib, w, h, 0, w, y, y, m_clrDot);
			drawVal(y, double(j) * 0.1);
		}
	}
}

void CAnalyzer::drawImpGrid(HEZDIMAGE hDib, HEZDFONT hFont, const float fmin, const float fmax, int w, int h, bool bGrid) {
	if (!bGrid) return;
	constexpr int n = 10;
	constexpr auto dn = 1.0 / double(n);
	auto getdV = [n, dn, fmin, fmax]() {
		const auto frng = fmax - fmin;
		auto dV = 0.0f;
		if (fmin < 0.0f && fmax > 0.0f) {
			auto dv = dV;
			for (int j = 0; j < n; j++) {
				const auto v = fmin + frng * float(j) / float(n);
				if (0 == j)
					dv = v;
				else if (fabs(v) < fabs(dv))
					dv = v;
			}
			dV = dv;
		}
		return std::make_pair(frng, dV);
	};
	const auto [frng, dV] = getdV();
	auto drawVal = [this, &hDib, &hFont, fmin, frng, dV, n](int y, const int j) {
		int nW = 0, nH = 0;
		const double v = fmin - dV + frng * double(j) / double(n);
		fmt::format_to(m_str, "{:.2f}{:c}", v, '\0');
		if ('-' == m_str[0] && 0 == atof(m_str))
			fmt::format_to(m_str, "{:.2f}{:c}", 0.0, '\0');
		ezd_text_size(hFont, m_str, 0, &nW, &nH);
		drawText(nH, hDib, hFont, m_str, 2, y, m_clrDb);
	};
	for (int j = 0; j < n; j++) {
		const int y = int(double(h) * (double(j) * dn - dV));
		drawDotScale(hDib, w, h, 0, w, y, y, m_clrDot);
		drawVal(y, j);
	}
}

void CAnalyzer::drawDotScale(HEZDIMAGE hDib, int w, int h, int x1, int x2, int y1, int y2, int nClr) const
{
	const int color = (1 == m_nBpp) ? 0xFFFFFF : (nClr < 0 ? m_clrDot : nClr);
	if (x1 == x2 && x1 >= 0 && x1 < w) { // vertical
		y1 = std::clamp(y1, 0, h);
		y2 = std::clamp(y2, 0, h);
		const int Y1 = std::min(y1, y2), Y2 = std::max(y1, y2), step = 4;
		for (int y = Y1; y < Y2; y += step)
			ezd_set_pixel(hDib, x1, y, color);
	}
	else if (y1 == y2 && y1 >= 0 && y1 < h) { // horizontal
		x1 = std::clamp(x1, 0, w);
		x2 = std::clamp(x2, 0, w);
		const int X1 = std::min(x1, x2), X2 = std::max(x1, x2), step = 4;
		for (int x = X1; x < X2; x += step)
			ezd_set_pixel(hDib, x, y1, color);
	}
}

std::tuple<std::error_code, float, float> CAnalyzer::saveImpImage(CImgSaveHelper& is, const imgOptions& io, const std::vector<float>& imp,
	const bool bCalcRange, float rngMin, float rngMax)
{
	auto err = std::make_error_code(std::errc::permission_denied);
	const auto [bOK, bFull, w, h, hDib, hFont] = initImage(io, imp);
	if (bOK) {
		auto respRange = [bCalcRange, rngMin, rngMax](const auto& imp, const float ext = 0.0f) {
			if (!bCalcRange)
				return std::make_pair(rngMin, rngMax);
			const auto [hmin, hmax] = std::minmax_element(imp.begin(), imp.end());
			auto rMin = *hmin, rMax = *hmax;
			if (rMin == rMax) {
				rMin -= 0.5; rMax += 0.5;
			}
			else if (ext != 0.0f) {
				const float add = rMax * ext - rMax;
				rMin -= add; rMax += add;
			}
			return std::make_pair(rMin, rMax);
		};
		const auto [rMin, rMax] = respRange(imp, 1.05f);
		rngMin = rMin; rngMax = rMax;
		drawImpGrid(hDib, hFont, rMin, rMax, w, h, io.drawGrid);
		const auto& src = imp;
		const int n = (int)src.size();
		const double kh = double(h), kr = 1.0 / (rMax - rMin);
		int x1 = -1, y1 = -1;
		if (n <= w) {
			const double kn = 1.0 / double(n), dw = double(w);
			for (int i = 0; i < n; i++) {
				const int x = int(kn * double(i) * dw),
					y = int(kh * (src[i] - rMin) * kr);
				if (0 == x) {
					x1 = x; y1 = y;
				}
				else {
					ezd_line(hDib, x1, y1, x, y, m_clrLine);
					x1 = x; y1 = y;
				}
			}
		}
		else {
			const double nd = double(n), kw = 1.0 / double(w);
			int i1 = -1;
			for (int x = 0; x < w; x++) {
				const int index = int(nd * double(x) * kw),
					y = int(kh * (src[index] - rMin) * kr);
				if (0 == x) {
					x1 = x; y1 = y; i1 = index;
				}
				else if (index != i1) {
					ezd_line(hDib, x1, y1, x, y, m_clrLine);
					x1 = x; y1 = y; i1 = index;
				}
			}
		}
		err = saveImage(is, io, hDib, hFont, w, h);
	}
	return std::make_tuple(err, rngMin, rngMax);
}