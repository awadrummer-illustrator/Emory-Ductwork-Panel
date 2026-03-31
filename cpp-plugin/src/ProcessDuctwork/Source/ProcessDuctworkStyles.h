#ifndef __ProcessDuctworkStyles_H__
#define __ProcessDuctworkStyles_H__

#include "ProcessDuctworkGeometry.h"

#include <vector>

struct DuctworkStyleStats
{
	size_t applied;
	size_t created;
	size_t skippedMissingStyle;
	size_t skippedNoSample;
	size_t skippedNonLineLayer;
};

namespace DuctworkStyles
{
	DuctworkStyleStats ApplyLineStyles(AIDocumentHandle document, const std::vector<DuctworkPath>& paths);
}

#endif // __ProcessDuctworkStyles_H__
