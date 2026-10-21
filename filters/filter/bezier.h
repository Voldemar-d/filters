#pragma once

#include <cstdint>
#include <utility>
#include <cstring>
#include <cmath>

class ContourNode {
public:
	ContourNode() = delete;
	ContourNode(const double x, const double y) {
		m_x = x; m_y = y;
	}
	void SetNext(ContourNode* prev) {
		m_next = prev;
	}
	void SetPrev(ContourNode* prev) {
		m_prev = prev;
	}
	auto GetNext() {
		return m_next;
	}
	auto GetPrev() {
		return m_prev;
	}
	void SetGuides(const double x1, const double y1, const double x2, const double y2);
	auto HasGuides() const {
		return m_hasGuides;
	}
	auto GetX() const {
		return m_x;
	}
	auto GetY() const {
		return m_y;
	}
	auto GetGuide1() const {
		return std::make_pair(m_x1, m_y1);
	}
	auto GetGuide2() const {
		return std::make_pair(m_x2, m_y2);
	}
private:
	ContourNode* m_next = nullptr;
	ContourNode* m_prev = nullptr;
	bool m_hasGuides = false;
	double m_x = -1, m_y = 0; // contour point
	double m_x1 = -1, m_y1 = 0; // first guide point
	double m_x2 = -1, m_y2 = 0; // second guide point
};

class Contour
{
public:
	~Contour() {
		Clear();
	}
	bool IsEmpty() const;
	auto TooShort() const {
		return (m_nodes < 2);
	}
	void Clear();
	auto GetHead() const {
		return m_head;
	}
	ContourNode* AddAfter(ContourNode* curnode, const double x, const double y);
	ContourNode* AddTail(const double x, const double y);
private:
	ContourNode* m_head = nullptr;
	ContourNode* m_tail = nullptr;
	uint32_t m_nodes = 0;
};

class CBezier
{
public:
	void CalcResp(const uint32_t fftsize, float* data, const uint32_t nbands, double* dB, const double bfreq, const double step);
	void CalcGain(const uint32_t nfrom, const uint32_t nto, double* dBfrom, double* dBto);
protected:
	void CreateGain(const uint32_t nbands, double* db) const;
	void CreateContour(const uint32_t nbands, double* dB, const double step);
	void PerformBezier();
	void BezierSample(ContourNode* curnode);
	bool IsSmall(const double x1, const double y1, const double x2, const double y2) const;
	void CreateResp(const uint32_t fftsize, float* data, const double bfreq);
	Contour m_contour;
	double m_minlen = -1;
};
