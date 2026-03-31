#ifndef __ProcessDuctworkConstants_H__
#define __ProcessDuctworkConstants_H__

#include <cstddef>

struct DuctworkConstants
{
	static const char* const kLineLayers[];
	static const size_t kLineLayerCount;

	static const char* const kPartLayers[];
	static const size_t kPartLayerCount;

	static const char* const kRegisterLayers[];
	static const size_t kRegisterLayerCount;

	static const char* const kDuctworkColorLayers[];
	static const size_t kDuctworkColorLayerCount;
};

#endif // __ProcessDuctworkConstants_H__
