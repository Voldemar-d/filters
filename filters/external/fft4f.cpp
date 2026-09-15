#include "fft4f.h"

void TFFTF::SetSize(int N) {
	const int n = 2 * N;
	m_n = n;
	m_w.resize(n * 5 / 4);
	const int sq = int(sqrt(float(n)) + 0.5) + 2;
	m_ip.resize(sq);
	m_ip[0] = 0;
	CDFT(nullptr);
}

void TFFTF::CDFT(float* data) {
	cdft(1, data);
}

void TFFTF::CDFTI(float* data) {
	cdft(-1, data);
}

void TFFTF::cdft(int isgn, float* data)
{
	const auto a = data;
	const int n = m_n;
	const auto ip = m_ip.data();
	const auto w = m_w.data();

	if (n > (ip[0] << 2)) {
		makewt(n >> 2, ip, w);
		return;
	}
	if (n > 4) {
		bitrv2(n, ip + 2, a);
	}
	if (n > 4 && isgn < 0) {
		int j;
		for (j = 1; j <= n - 1; j += 2) {
			a[j] = -a[j];
		}
		cftsub(n, a, w);
		for (j = 1; j <= n - 1; j += 2) {
			a[j] = -a[j];
		}
	}
	else {
		cftsub(n, a, w);
	}
}

// -------- initializing routines --------

void TFFTF::makewt(int nw, int* ip, float* w)
{
	int nwh, j;
	float delta, x, y;

	ip[0] = nw;
	ip[1] = 1;
	if (nw > 2) {
		nwh = nw >> 1;
		delta = float(atan(1.0) / double(nwh));
		w[0] = 1;
		w[1] = 0;
		w[nwh] = (float)cos(delta * nwh);
		w[nwh + 1] = w[nwh];
		for (j = 2; j <= nwh - 2; j += 2) {
			x = (float)cos(delta * j);
			y = (float)sin(delta * j);
			w[j] = x;
			w[j + 1] = y;
			w[nw - j] = y;
			w[nw - j + 1] = x;
		}
		bitrv2(nw, ip + 2, w);
	}
}

// -------- child routines --------

void TFFTF::bitrv2(int n, int* ip, float* a)
{
	int j, j1, k, k1, l, m, m2;
	float xr, xi;

	ip[0] = 0;
	l = n;
	m = 1;
	while ((m << 2) < l) {
		l >>= 1;
		for (j = 0; j <= m - 1; j++) {
			ip[m + j] = ip[j] + l;
		}
		m <<= 1;
	}
	if ((m << 2) > l) {
		for (k = 1; k <= m - 1; k++) {
			for (j = 0; j <= k - 1; j++) {
				j1 = (j << 1) + ip[k];
				k1 = (k << 1) + ip[j];
				xr = a[j1];
				xi = a[j1 + 1];
				a[j1] = a[k1];
				a[j1 + 1] = a[k1 + 1];
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
		}
	}
	else {
		m2 = m << 1;
		for (k = 1; k <= m - 1; k++) {
			for (j = 0; j <= k - 1; j++) {
				j1 = (j << 1) + ip[k];
				k1 = (k << 1) + ip[j];
				xr = a[j1];
				xi = a[j1 + 1];
				a[j1] = a[k1];
				a[j1 + 1] = a[k1 + 1];
				a[k1] = xr;
				a[k1 + 1] = xi;
				j1 += m2;
				k1 += m2;
				xr = a[j1];
				xi = a[j1 + 1];
				a[j1] = a[k1];
				a[j1 + 1] = a[k1 + 1];
				a[k1] = xr;
				a[k1 + 1] = xi;
			}
		}
	}
}

void TFFTF::cftsub(int n, float* a, float* w)
{
	int j, j1, j2, j3, k, k1, ks, l, m;
	float wk1r, wk1i, wk2r, wk2i, wk3r, wk3i;
	float x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

	l = 2;
	while ((l << 1) < n) {
		m = l << 2;
		for (j = 0; j <= l - 2; j += 2) {
			j1 = j + l;
			j2 = j1 + l;
			j3 = j2 + l;
			x0r = a[j] + a[j1];
			x0i = a[j + 1] + a[j1 + 1];
			x1r = a[j] - a[j1];
			x1i = a[j + 1] - a[j1 + 1];
			x2r = a[j2] + a[j3];
			x2i = a[j2 + 1] + a[j3 + 1];
			x3r = a[j2] - a[j3];
			x3i = a[j2 + 1] - a[j3 + 1];
			a[j] = x0r + x2r;
			a[j + 1] = x0i + x2i;
			a[j2] = x0r - x2r;
			a[j2 + 1] = x0i - x2i;
			a[j1] = x1r - x3i;
			a[j1 + 1] = x1i + x3r;
			a[j3] = x1r + x3i;
			a[j3 + 1] = x1i - x3r;
		}
		if (m < n) {
			wk1r = w[2];
			for (j = m; j <= l + m - 2; j += 2) {
				j1 = j + l;
				j2 = j1 + l;
				j3 = j2 + l;
				x0r = a[j] + a[j1];
				x0i = a[j + 1] + a[j1 + 1];
				x1r = a[j] - a[j1];
				x1i = a[j + 1] - a[j1 + 1];
				x2r = a[j2] + a[j3];
				x2i = a[j2 + 1] + a[j3 + 1];
				x3r = a[j2] - a[j3];
				x3i = a[j2 + 1] - a[j3 + 1];
				a[j] = x0r + x2r;
				a[j + 1] = x0i + x2i;
				a[j2] = x2i - x0i;
				a[j2 + 1] = x0r - x2r;
				x0r = x1r - x3i;
				x0i = x1i + x3r;
				a[j1] = wk1r * (x0r - x0i);
				a[j1 + 1] = wk1r * (x0r + x0i);
				x0r = x3i + x1r;
				x0i = x3r - x1i;
				a[j3] = wk1r * (x0i - x0r);
				a[j3 + 1] = wk1r * (x0i + x0r);
			}
			k1 = 1;
			ks = -1;
			for (k = (m << 1); k <= n - m; k += m) {
				k1++;
				ks = -ks;
				wk1r = w[k1 << 1];
				wk1i = w[(k1 << 1) + 1];
				wk2r = ks * w[k1];
				wk2i = w[k1 + ks];
				wk3r = wk1r - 2 * wk2i * wk1i;
				wk3i = 2 * wk2i * wk1r - wk1i;
				for (j = k; j <= l + k - 2; j += 2) {
					j1 = j + l;
					j2 = j1 + l;
					j3 = j2 + l;
					x0r = a[j] + a[j1];
					x0i = a[j + 1] + a[j1 + 1];
					x1r = a[j] - a[j1];
					x1i = a[j + 1] - a[j1 + 1];
					x2r = a[j2] + a[j3];
					x2i = a[j2 + 1] + a[j3 + 1];
					x3r = a[j2] - a[j3];
					x3i = a[j2 + 1] - a[j3 + 1];
					a[j] = x0r + x2r;
					a[j + 1] = x0i + x2i;
					x0r -= x2r;
					x0i -= x2i;
					a[j2] = wk2r * x0r - wk2i * x0i;
					a[j2 + 1] = wk2r * x0i + wk2i * x0r;
					x0r = x1r - x3i;
					x0i = x1i + x3r;
					a[j1] = wk1r * x0r - wk1i * x0i;
					a[j1 + 1] = wk1r * x0i + wk1i * x0r;
					x0r = x1r + x3i;
					x0i = x1i - x3r;
					a[j3] = wk3r * x0r - wk3i * x0i;
					a[j3 + 1] = wk3r * x0i + wk3i * x0r;
				}
			}
		}
		l = m;
	}
	if (l < n) {
		for (j = 0; j <= l - 2; j += 2) {
			j1 = j + l;
			x0r = a[j] - a[j1];
			x0i = a[j + 1] - a[j1 + 1];
			a[j] += a[j1];
			a[j + 1] += a[j1 + 1];
			a[j1] = x0r;
			a[j1 + 1] = x0i;
		}
	}
}
