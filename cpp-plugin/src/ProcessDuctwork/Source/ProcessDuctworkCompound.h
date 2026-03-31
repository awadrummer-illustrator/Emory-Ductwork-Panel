#ifndef __ProcessDuctworkCompound_H__
#define __ProcessDuctworkCompound_H__

#include "ProcessDuctworkConnections.h"
#include "ProcessDuctworkGeometry.h"

#include <cstddef>
#include <vector>

struct DuctworkCompoundStats
{
	size_t components;
	size_t compoundsCreated;
	size_t skippedSingle;
	size_t skippedFailed;
	size_t movedToLayer;
	size_t metaWritten;
};

namespace DuctworkCompound
{
	// Release all compound paths on ductwork layers, returning child paths to their original layer
	size_t ReleaseCompoundPaths(AIDocumentHandle document);

	DuctworkCompoundStats MergeConnectedPaths(const std::vector<DuctworkPath>& paths,
		const std::vector<DuctworkConnection>& connections,
		std::vector<AIArtHandle>& outCompounds);
}

#endif // __ProcessDuctworkCompound_H__
