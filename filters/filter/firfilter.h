#pragma once

#include "../external/fft4f.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <system_error>

#define MIN_FIR_LENGTH 15
#define MAX_FIR_LENGTH 65535

// Compute Bessel function Izero(y) using a series approximation
inline double izero(double y) {
	double s = 1.0, ds = 1.0, d = 0.0;
	do {
		d += 2.0;
		ds = ds * (y * y) / (d * d);
		s = s + ds;
	} while (ds > 1E-7 * s);
	return s;
}

class CFIRFilter {
public:
	CFIRFilter() {}
	CFIRFilter(const CFIRFilter& other) {
		// copy filter impulse response
		m_flt = other.m_flt;
	}
	std::error_code CalcLowPass(bool bKaiser, int& nLength, int nFreq, int nSampleRate, int nFFTsz = 0) {
		auto err = CheckFreqLen(nLength, nFreq, nSampleRate);
		if (err) return err;
		if (bKaiser) {
			m_flt.resize(nLength);
			KaiserLowHighPass<true>(m_flt.data(), nLength, double(nFreq) / double(nSampleRate), false);
		}
		else { // no Kaiser window
			m_flt.resize(nLength);
			KaiserLowHighPass<false>(m_flt.data(), nLength, double(nFreq) / double(nSampleRate), false);
		}
		return err;
	}
	std::error_code CalcHighPass(bool bKaiser, int& nLength, int nFreq, int nSampleRate, int nFFTsz = 0) {
		auto err = CheckFreqLen(nLength, nFreq, nSampleRate);
		if (err) return err;
		if (bKaiser) {
			m_flt.resize(nLength);
			KaiserLowHighPass<true>(m_flt.data(), nLength, double(nFreq) / double(nSampleRate), true);
		}
		else { // no Kaiser window
			m_flt.resize(nLength);
			KaiserLowHighPass<false>(m_flt.data(), nLength, double(nFreq) / double(nSampleRate), true);
		}
		return err;
	}
	// calculate filter using inverse FFT
	template<bool bLowPass>
	std::error_code InvLowHighPass(bool bKaiser, int& nLength, int nFreq, int nSampleRate, int nFFTsz = 0) {
		auto err = CheckFreqLen(nLength, nFreq, nSampleRate);
		if (err) return err;
		int fftSz;
		if constexpr (bLowPass)
			fftSz = initFFT(nLength, nFFTsz, false);
		else
			fftSz = initFFT(nLength, nFFTsz, true);
		int i, j = (int)m_buf.size() - 2;
		const int nF = int(double(m_buf.size()) * double(nFreq) / double(nSampleRate));
		for (i = 0; i <= nF; i += 2, j -= 2) {
			if constexpr (bLowPass) {
				m_buf[i] = m_buf[j] = 1.0f;
			}
			else {
				m_buf[i] = m_buf[j] = 0.0f;
			}
		}
		calcFilter(fftSz, nLength);
		if (bKaiser) { // apply Kaiser window
			const int Np = nLength / 2; // (N-1)/2 for odd N
			const auto [ni, beta, vb] = KaiserConst(Np);
			double dk, alpha, y, t, w;
			int i, j, k;
			auto h = m_flt.data();
			for (k = 0, i = j = Np; k <= Np; k++, i++, j--) {
				dk = double(k);
				alpha = dk * ni;
				t = 1.0 - alpha * alpha;
				if (t < 0) t = 0;
				y = beta * sqrt(t);
				w = vb * izero(y);
				h[i] = float(h[i] * w);
				h[j] = float(h[j] * w);
			}
		}
		return err;
	}
	auto& GetFilter() const {
		return m_flt;
	}
	float Apply(float in, bool bStart = false) {
		const int len = (int)m_flt.size();
		if (m_data.empty() || bStart) {
			m_data.resize(len);
			std::fill(m_data.begin(), m_data.end(), 0.0f);
			m_pos = 0;
		}
		const auto data = m_data.data();
		data[m_pos++] = in;
		if (m_pos >= len)
			m_pos = 0;

		float out = 0.0f;

		auto d = data + m_pos;
		auto t = m_flt.data() + len - 1;
		int i, ln = len - m_pos;
		for (i = 0; i < ln; i++)
			out += *t-- * *d++;

		d = data;
		ln = m_pos;
		for (i = 0; i < ln; i++)
			out += *t-- * *d++;

		return out;
	}
	float ApplySym(float in, bool bStart = false) {
		const int len = (int)m_flt.size();
		if (m_data.empty() || bStart) {
			m_data.resize(len);
			std::fill(m_data.begin(), m_data.end(), 0.0f);
			m_pos = 0;
		}
		const auto data = m_data.data();
		data[m_pos++] = in;
		if (m_pos >= len)
			m_pos = 0;

		float out = 0.0f, sum;

		auto dp1 = data + m_pos - 1;
		if (m_pos == 0)
			dp1 = data + len - 1;
		auto dp2 = data + m_pos;

		auto t = m_flt.data();
		const auto dlast = data + len - 1;

		do {
			sum = (*dp2++) + (*dp1--);
			out += *t++ * sum;
			if (dp2 > dlast)
				dp2 = data;
			if (dp1 < data)
				dp1 = dlast;
		} while (dp2 != dp1);

		out += (*t) * (*dp2);
		return out;
	}
private:
	std::tuple<double, double, double> KaiserConst(const int Np) const { // (N-1)/2 for odd N
		const double ni = 1.0 / double(Np);
		constexpr double att = 96.0;
		double beta = 0;  // value of beta if att < 21
		if constexpr (att >= 50)
			beta = .1102 * (att - 8.71);
		if constexpr (att < 50 && att >= 21)
			beta = .5842 * pow((att - 21), 0.4) + .07886 * (att - 21);
		const double vb = 1.0 / izero(beta);
		return std::make_tuple(ni, beta, vb);
	};
	template<bool bKaiser>
	int KaiserLowHighPass(float* h, int n, const double freq, const bool bHighPass) const
	{
		if (!(n % 2)) n--;
		const int Np = n / 2; // (N-1)/2 for odd N
		const auto [ni, beta, vb] = KaiserConst(Np);
		double alpha, y, t, ck, dk, w;
		if (bHighPass) {
			const int Np = n / 2;
			std::vector<double> A(Np + 1);
			A[0] = 2.0 * (0.5 - freq);
			int j; constexpr double ratio = 1.0 / M_PI, tpi = 2.0 * M_PI;
			for (j = 1; j <= Np; j++) {
				dk = double(j);
				A[j] = ratio * (sin(dk * M_PI) - sin(tpi * dk * freq)) / dk;
			}
			for (j = 0; j <= Np; j++)
			{
				if constexpr (bKaiser) {
					dk = double(j);
					alpha = dk * ni;
					t = 1.0 - alpha * alpha;
					y = beta * sqrt(t);
					w = vb * izero(y);
					h[Np + j] = float(A[j] * w);
				}
				else
					h[Np + j] = float(A[j]);
			}
			for (j = 0; j < Np; j++)
				h[j] = h[n - 1 - j];
		}
		else { // LowPass
			const double ratio = 0.5 / freq;
			const double pi_r = M_PI / ratio;
			int i, j, k;
			for (k = 0, i = j = Np; k <= Np; k++, i++, j--) {
				dk = double(k);
				if constexpr (bKaiser) {
					alpha = dk * ni;
					t = 1.0 - alpha * alpha;
					if (t < 0) t = 0;
					y = beta * sqrt(t);
					w = vb * izero(y);
				}
				if (0 == k)
					ck = 1.0;
				else
					ck = ratio * sin(dk * pi_r) / (dk * M_PI);
				if constexpr (bKaiser) {
					h[i] = h[j] = float(w * ck / ratio);
				}
				else {
					h[i] = h[j] = float(ck / ratio);
				}
			}
		}
		return n;
	}
	std::error_code CheckFreqLen(int& nLength, const int nFreq, const int nSampleRate) const {
		if (nFreq < 10 || nFreq > nSampleRate * 499 / 1000 || nLength < MIN_FIR_LENGTH || nLength > MAX_FIR_LENGTH)
			return std::make_error_code(std::errc::invalid_argument);
		if (0 == nLength % 2)
			nLength++;
		return std::make_error_code(std::errc());
	}
	int getFFTsize(const int nSamples) const
	{
		int fsz = 2;
		while (fsz < nSamples)
			fsz <<= 1;
		if (fsz > nSamples)
			fsz >>= 1;
		return fsz;
	}
	int initFFT(const int nLength, const int nFFTsz, const bool bFill1) {
		const int fftSz = getFFTsize(std::max(nLength * 2, nFFTsz));
		m_buf.resize((size_t)fftSz * 2);
		if (bFill1) {
			auto it = m_buf.begin();
			for (int i = 0; i < nFFTsz * 2; i += 2) {
				*it++ = 1.0f;
				*it++ = 0.0f;
			}
		}
		else
			std::fill(m_buf.begin(), m_buf.end(), 0.0f);
		m_fft.SetSize(fftSz);
		return fftSz;
	}
	void calcFilter(const int nFFTsz, const int firlen) {
		auto data = m_buf.data();
		m_fft.CDFTI(data);
		const double r = 1.0 / double(nFFTsz);
		auto s1 = data;
		m_flt.resize(firlen);
		auto fh = m_flt.data();
		const int Np = firlen / 2; // (N-1)/2 for odd N
		fh[Np] = float((*s1) * r);
		s1 += 2;
		auto d1 = fh + Np - 1;
		auto d2 = fh + Np + 1;
		for (int i = 0; i < Np; i++) {
			*d1 = *d2 = float((*s1) * r);
			d1--; d2++; s1 += 2;
		}
		m_buf.clear();
		m_buf.shrink_to_fit();
	}
	int m_pos = 0;
	std::vector<float> m_flt, m_buf, m_data;
	TFFTF m_fft;
};
