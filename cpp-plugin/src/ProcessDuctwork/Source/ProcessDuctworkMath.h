#ifndef __ProcessDuctworkMath_H__
#define __ProcessDuctworkMath_H__

#include "ProcessDuctworkGeometry.h"

namespace DuctworkMath
{
	double Dist2(const DuctworkPoint& a, const DuctworkPoint& b);
	double Dist(const DuctworkPoint& a, const DuctworkPoint& b);
	double Dot(double ax, double ay, double bx, double by);
	bool SegmentIntersection(const DuctworkPoint& a1, const DuctworkPoint& a2,
		const DuctworkPoint& b1, const DuctworkPoint& b2, DuctworkPoint& out);
	DuctworkPoint ClosestPointOnSegment(const DuctworkPoint& a, const DuctworkPoint& b, const DuctworkPoint& p, double& outT);
}

#endif // __ProcessDuctworkMath_H__
