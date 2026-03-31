#include "IllustratorSDK.h"
#include "ProcessDuctworkConstants.h"

const char* const DuctworkConstants::kLineLayers[] = {
	"Green Ductwork",
	"Light Green Ductwork",
	"Blue Ductwork",
	"Orange Ductwork",
	"Light Orange Ductwork",
	"Thermostat Lines"
};

const size_t DuctworkConstants::kLineLayerCount = sizeof(DuctworkConstants::kLineLayers) / sizeof(DuctworkConstants::kLineLayers[0]);

const char* const DuctworkConstants::kPartLayers[] = {
	"Thermostats",
	"Units",
	"Secondary Exhaust Registers",
	"Exhaust Registers",
	"Orange Register",
	"Rectangular Registers",
	"Square Registers",
	"Circular Registers"
};

const size_t DuctworkConstants::kPartLayerCount = sizeof(DuctworkConstants::kPartLayers) / sizeof(DuctworkConstants::kPartLayers[0]);

const char* const DuctworkConstants::kRegisterLayers[] = {
	"Square Registers",
	"Rectangular Registers",
	"Circular Registers",
	"Exhaust Registers",
	"Secondary Exhaust Registers",
	"Orange Register"
};

const size_t DuctworkConstants::kRegisterLayerCount = sizeof(DuctworkConstants::kRegisterLayers) / sizeof(DuctworkConstants::kRegisterLayers[0]);

const char* const DuctworkConstants::kDuctworkColorLayers[] = {
	"Green Ductwork",
	"Light Green Ductwork",
	"Blue Ductwork",
	"Orange Ductwork",
	"Light Orange Ductwork"
};

const size_t DuctworkConstants::kDuctworkColorLayerCount =
	sizeof(DuctworkConstants::kDuctworkColorLayers) / sizeof(DuctworkConstants::kDuctworkColorLayers[0]);
