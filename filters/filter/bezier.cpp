#include "bezier.h"

inline float dB2GainF(double dB)
{
	return float(pow(10.0, dB / 20.0));
}

inline double FreqOct(double freq, double oct)
{
	return freq * exp(log(2.0) * oct);
}

//////////////////////////////////////////////////////////////////////
// Contour
//////////////////////////////////////////////////////////////////////

ContourNode* Contour::AddTail(const double x, const double y)
{
	auto node = new ContourNode(x, y);
	if (nullptr == m_head) {
		m_head = m_tail = node;
	}
	else {
		m_tail->SetNext(node);
		node->SetPrev(m_tail);
		m_tail = m_tail->GetNext();
	}
	m_nodes++;
	return node;
}

ContourNode* Contour::AddAfter(ContourNode* curnode, const double x, const double y)
{
	auto node = new ContourNode(x, y);
	if (curnode->GetNext() != nullptr) {
		curnode->GetNext()->SetPrev(node);
		node->SetNext(curnode->GetNext());
	}
	curnode->SetNext(node);
	node->SetPrev(curnode);
	if (curnode == m_tail)
		m_tail = node;
	m_nodes++;
	return node;
}

void Contour::Clear()
{
	auto node = m_head;
	auto curnode = m_head;
	while (node != nullptr) {
		curnode = node;
		node = node->GetNext();
		delete curnode;
	}
	m_head = m_tail = nullptr;
	m_nodes = 0;
}

bool Contour::IsEmpty() const
{
	return (m_nodes == 0);
}

//////////////////////////////////////////////////////////////////////
// ContourNode
//////////////////////////////////////////////////////////////////////

void ContourNode::SetGuides(const double x1, const double y1, const double x2, const double y2)
{
	m_hasGuides = true;
	m_x1 = x1;
	m_y1 = y1;
	m_x2 = x2;
	m_y2 = y2;
}

//////////////////////////////////////////////////////////////////////
// CBezier
//////////////////////////////////////////////////////////////////////

void CBezier::CalcResp(const uint32_t fftsize, float* data, const uint32_t nbands, double* dB, const double bfreq, const double step)
{
	CreateContour(nbands, dB, step);
	const double minint = step / 24.0;
	constexpr double mindb = 0.1;
	m_minlen = minint * minint + mindb * mindb;
	PerformBezier();
	CreateResp(fftsize, data, bfreq);
}

void CBezier::CreateContour(const uint32_t nbands, double* dB, const double step)
{
	// create m_contour, freq in octaves from base freq
	m_contour.Clear();
	uint32_t band = 0;
	const double bw = step * 0.125;
	double f = 0, db = dB[band++], dbnext, fnext = step;
	double f1, db1, f2, db2; // guide points
	// first point at freq = 0 oct (base freq)
	auto curnode = m_contour.AddTail(f, db);
	while (band < nbands) {
		dbnext = dB[band];
		fnext = f + step;
		if (dbnext == db) // no guide points
			curnode = m_contour.AddTail(fnext, dbnext);
		else { // add a point to make bandwidth
			curnode = m_contour.AddTail(f + bw, db);
			f1 = f2 = f + step * 0.5; // middle between bands
			db1 = db; db2 = dbnext;
			// set guide points
			curnode->SetGuides(f1, db1, f2, db2);
			curnode = m_contour.AddTail(fnext - bw, dbnext);
		}
		db = dbnext; f = fnext;
		band++;
	}
	// edge of last highpass band
	m_contour.AddTail(fnext, db);
}

void CBezier::PerformBezier()
{
	if (m_contour.IsEmpty())
		return;
	auto curnode = m_contour.GetHead();
	while (curnode != nullptr) {
		if (curnode->HasGuides())
			BezierSample(curnode);
		curnode = curnode->GetNext();
	}
}

void CBezier::BezierSample(ContourNode* curnode)
{
	const auto next = curnode->GetNext();
	if (next == nullptr)
		return;
	// first key point
	auto x0 = curnode->GetX(), y0 = curnode->GetY();
	// second key point
	auto x3 = next->GetX(), y3 = next->GetY();
	if (IsSmall(x0, y0, x3, y3))
		return;
	// first guide point
	auto [x1, y1] = curnode->GetGuide1();
	// second guide point
	auto [x2, y2] = curnode->GetGuide2();
	// calculate first middle point
	x0 = (x0 + x1) * 0.5;
	y0 = (y0 + y1) * 0.5;
	// calculate second middle point
	x3 = (x2 + x3) * 0.5;
	y3 = (y2 + y3) * 0.5;
	// calculate guide middle point
	double x = (x1 + x2) * 0.5;
	double y = (y1 + y2) * 0.5;
	// calculate new guide points
	x1 = (x0 + x) * 0.5;
	y1 = (y0 + y) * 0.5;
	x2 = (x3 + x) * 0.5;
	y2 = (y3 + y) * 0.5;
	// calculate new middle point
	x = (x1 + x2) * 0.5;
	y = (y1 + y2) * 0.5;
	// add new node at new middle point
	auto newnode = m_contour.AddAfter(curnode, x, y);
	curnode->SetGuides(x0, y0, x1, y1);
	BezierSample(curnode);
	newnode->SetGuides(x2, y2, x3, y3);
	BezierSample(newnode);
	return;
}

bool CBezier::IsSmall(const double x1, const double y1, const double x2, const double y2) const
{
	return ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) < m_minlen);
}

void CBezier::CreateResp(const uint32_t fftsize, float* data, const double bfreq)
{
	memset(data, fftsize * 2 * sizeof(float), 0);
	auto curnode = m_contour.GetHead();
	if (nullptr == curnode || m_contour.TooShort())
		return;
	const auto ln2 = 1.0 / log(2.0);
	// lowpass band freq should be equal to bfreq
	auto f = curnode->GetX(), db = curnode->GetY();
	double fnext, dbnext, fk, fc, fint, foct, dbcur;
	curnode = curnode->GetNext();
	uint32_t index = 0, index1 = uint32_t(FreqOct(bfreq, f) * double(fftsize) + 0.5), i, j;
	// symmetric data pointers
	auto s1 = data, s2 = data + (fftsize - 1) * 2;
	// current sample
	auto s = dB2GainF(db);
	// fill lowpass band
	while (index < index1) {
		*s1 = *s2 = s;
		s1 += 2; s2 -= 2;
		index++;
	}
	const auto fftsz2 = fftsize / 2;
	// fill middle bands
	while (curnode != nullptr) {
		// get current values
		fnext = curnode->GetX();
		dbnext = curnode->GetY();
		index1 = uint32_t(FreqOct(bfreq, fnext) * double(fftsize) + 0.5);
		if (index1 >= fftsz2) // limit frequency
			break;
		if (index1 > index) {
			// fill curve segment
			if (db == dbnext) { // flat
				s = dB2GainF(db);
				for (i = index; i < index1; i++) {
					*s1 = *s2 = s;
					s1 += 2; s2 -= 2;
				}
			}
			else {
				// linear freq increment
				fk = (FreqOct(bfreq, fnext) - FreqOct(bfreq, f)) / double(index1 - index);
				// interval between two key points in octaves
				foct = 1.0 / (fnext - f);
				// current linear freq
				fc = f;
				for (i = index, j = 0; i < index1; i++, j++) {
					// interval between key freq and current freq in octaves
					fint = log(fc / f) * ln2;
					// calculate current db
					dbcur = db + (dbnext - db) * fint * foct;
					// fill-in current sample
					s = dB2GainF(dbcur);
					*s1 = *s2 = s;
					s1 += 2; s2 -= 2;
					// increment current freq
					fc += fk;
				}
			}
		}
		// finish cycle
		f = fnext; index = index1;
		db = dbnext;
		curnode = curnode->GetNext();
	}
	// fill highpass band
	s = dB2GainF(db);
	while (index < fftsz2) {
		*s1 = *s2 = s;
		s1 += 2; s2 -= 2;
		index++;
	}
}

void CBezier::CalcGain(const uint32_t nfrom, const uint32_t nto, double* dBfrom, double* dBto)
{
	const double step = 10.0 / double(nfrom);
	CreateContour(nfrom, dBfrom, step);
	const double minint = step / 24.0;
	constexpr double mindb = 0.1;
	m_minlen = minint * minint + mindb * mindb;
	PerformBezier();
	CreateGain(nto, dBto);
	if (nto > nfrom) {
		if (nto == 20)
			dBto[nto - 1] = dBto[nto - 2];
		if (nto == 30)
			dBto[nto - 1] = dBto[nto - 2] = dBto[nto - 3];
	}
}

void CBezier::CreateGain(const uint32_t nbands, double* db) const
{
	memset(db, nbands * sizeof(double), 0);
	auto curnode = m_contour.GetHead();
	if (nullptr == curnode || m_contour.TooShort())
		return;
	uint32_t i = 0; // current band number
	auto dB1 = curnode->GetY();
	// first band gain
	db[i++] = dB1;
	double dB2, dB, step = 10.0 / double(nbands);
	double oct = step;
	while (curnode != nullptr) {
		// find appropriate curve segment
		while (curnode != nullptr && curnode->GetX() < oct) {
			curnode = curnode->GetNext();
		}
		if (curnode != nullptr) {
			dB1 = curnode->GetPrev()->GetY(); dB2 = curnode->GetY();
			dB = dB1 + (dB2 - dB1) * (oct - curnode->GetPrev()->GetX());
			if (i < nbands) db[i++] = dB;
		}
		oct += step;
	}
}
