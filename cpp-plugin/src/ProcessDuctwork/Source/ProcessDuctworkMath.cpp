#include "IllustratorSDK.h"
#include "ProcessDuctworkMath.h"

#include <cmath>

double DuctworkMath::Dist2(const DuctworkPoint& a, const DuctworkPoint& b)
{
	const double dx = a.x - b.x;
	const double dy = a.y - b.y;
	return (dx * dx) + (dy * dy);
}

double DuctworkMath::Dist(const DuctworkPoint& a, const DuctworkPoint& b)
{
	return std::sqrt(Dist2(a, b));
}

double DuctworkMath::Dot(double ax, double ay, double bx, double by)
{
	return (ax * bx) + (ay * by);
}

bool DuctworkMath::SegmentIntersection(const DuctworkPoint& a1, const DuctworkPoint& a2,
	const DuctworkPoint& b1, const DuctworkPoint& b2, DuctworkPoint& out)
{
	const double dax = a2.x - a1.x;
	const double day = a2.y - a1.y;
	const double dbx = b2.x - b1.x;
	const double dby = b2.y - b1.y;
	const double denom = (dax * dby) - (day * dbx);
	if (std::fabs(denom) < 1e-10) {
		return false;
	}
	const double dx = a1.x - b1.x;
	const double dy = a1.y - b1.y;
	const double t = ((dbx * dy) - (dby * dx)) / denom;
	const double u = ((dax * dy) - (day * dx)) / denom;
	if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
		out.x = a1.x + t * dax;
		out.y = a1.y + t * day;
		return true;
	}
	return false;
}

DuctworkPoint DuctworkMath::ClosestPointOnSegment(const DuctworkPoint& a, const DuctworkPoint& b, const DuctworkPoint& p, double& outT)
{
	const double abx = b.x - a.x;
	const double aby = b.y - a.y;
	const double apx = p.x - a.x;
	const double apy = p.y - a.y;
	const double ab2 = (abx * abx) + (aby * aby);
	double t = 0.0;
	if (ab2 > 1e-12) {
		t = ((apx * abx) + (apy * aby)) / ab2;
		if (t < 0.0) {
			t = 0.0;
		} else if (t > 1.0) {
			t = 1.0;
		}
	}
	outT = t;
	DuctworkPoint result;
	result.x = a.x + t * abx;
	result.y = a.y + t * aby;
	return result;
}
