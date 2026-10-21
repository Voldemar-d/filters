#pragma once

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Compute Bessel function Izero(y) using a series approximation
inline double izero(double y) {
	double s = 1.0, ds = 1.0, d = 0.0;
	do {
		d += 2;
		ds = ds * (y * y) / (d * d);
		s += ds;
	} while (ds > 1E-10 * s);
	return s;
}

enum class eWndType {
	none,
	Hann,
	Hamming,
	Bartlett,
	Gauss,
	Kaiser,
	Blackman1,
	Blackman2
};

template<typename T>
void ApplyWindow(T* h, const int n, const eWndType type)
{
	double w;
	const double k = M_PI / double(n - 1);
	const int hs = n / 2;
	int i;
	switch (type) {
	case eWndType::Hann:
		for (i = -hs; i < hs; i++) {
			w = 0.5 + 0.5 * cos(k * double(2.0 * double(i) + 1.0));
			h[i + hs] *= T(w);
		}
		break;
	case eWndType::Hamming:
		for (i = -hs; i < hs; i++) {
			w = 0.54 + 0.46 * cos(k * double(2.0 * double(i) + 1.0));
			h[i + hs] *= T(w);
		}
		break;
	case eWndType::Bartlett:
	{
		const double N1 = 1.0 / double(n + 1);
		for (i = -hs; i < hs; i++) {
			w = 1.0 - fabs(2.0 * double(i) + 1.0) * N1;
			h[i + hs] *= T(w);
		}
	}
	break;
	case eWndType::Gauss:
	{
		const double m = 5.0 / double(n);
		for (i = -hs; i < hs; i++) {
			w = exp(-0.5 * (m * double(i)) * (m * double(i)));
			h[i + hs] *= T(w);
		}
	}
	break;
	case eWndType::Kaiser:
	{
		const int hs = n / 2;
		int n1 = hs;
		if (n % 2 == 0) n1--;
		constexpr double beta = 8.96;
		const double vb = 1.0 / izero(beta);
		const double ni = 1.0 / double(hs);
		double alpha, y, t, dk, w;
		int i, j, k;
		for (k = 0, i = j = hs; k <= n1; k++, i++, j--) {
			dk = double(k);
			alpha = dk * ni;
			t = 1.0 - alpha * alpha;
			if (t < 0) t = 0;
			y = beta * sqrt(t);
			w = vb * izero(y);
			h[i] *= T(w);
			h[j] *= T(w);
		}
		if (n % 2 == 0) {
			dk = double(hs);
			alpha = dk * ni;
			t = 1.0 - alpha * alpha;
			if (t < 0) t = 0;
			y = beta * sqrt(t);
			w = vb * izero(y);
			h[0] = T(w);
		}
	}
	break;
	case eWndType::Blackman1:
	{
		const double k1 = 2 * M_PI / double(n - 1);
		const double k2 = 2 * k1;
		constexpr double alpha = -0.16;
		constexpr double a1 = (alpha + 1) * 0.5;
		constexpr double a2 = alpha * 0.5;
		for (i = 0; i < n; i++) {
			w = a1 - 0.5 * cos(k1 * double(i)) - a2 * cos(k2 * double(i));
			h[i] *= T(w);
		}
	}
	break;
	case eWndType::Blackman2:
	{
		const double N1 = 1.0 / (double(n) - 1.0);
		const double k1 = 2 * M_PI * N1;
		const double k2 = 2 * k1;
		double alpha = sin(M_PI * N1) / sin(2 * M_PI * N1);
		alpha = -alpha * alpha;
		const double a1 = (alpha + 1) * 0.5;
		const double a2 = alpha * 0.5;
		for (i = 0; i < n; i++) {
			w = a1 - 0.5 * cos(k1 * double(i)) - a2 * cos(k2 * double(i));
			h[i] *= T(w);
		}
		break;
	}
	default:
		break;
	}
}