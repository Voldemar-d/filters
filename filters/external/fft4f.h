#pragma once

#include <cmath>
#include <vector>

class TFFTF {
public:
	TFFTF() {}
	void SetSize(int N);
	void CDFT(float* data);
	void CDFTI(float* data);
protected:
	void cdft(int isgn, float* data);
	void makewt(int nw, int* ip, float* w);
	void bitrv2(int n, int* ip, float* a);
	void cftsub(int n, float* a, float* w);
	int m_n = 0;
	std::vector<int> m_ip;
	std::vector<float> m_w;
};
