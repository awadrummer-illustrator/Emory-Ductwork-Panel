#include "IllustratorSDK.h"
#include "SelectDuctworkPartsSuites.h"

extern "C" {
	SPBlocksSuite* sSPBlocks = NULL;
	AIMenuSuite* sAIMenu = NULL;
	AIUnicodeStringSuite* sAIUnicodeString = NULL;
	AIArtSuite* sAIArt = NULL;
	AIArtSetSuite* sAIArtSet = NULL;
	AILayerSuite* sAILayer = NULL;
	AIDocumentSuite* sAIDocument = NULL;
}

ImportSuite gImportSuites[] =
{
	kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
	kAIMenuSuite, kAIMenuSuiteVersion, &sAIMenu,
	kAIUnicodeStringSuite, kAIUnicodeStringVersion, &sAIUnicodeString,
	kAIArtSuite, kAIArtSuiteVersion, &sAIArt,
	kAIArtSetSuite, kAIArtSetSuiteVersion, &sAIArtSet,
	kAILayerSuite, kAILayerSuiteVersion, &sAILayer,
	kAIDocumentSuite, kAIDocumentSuiteVersion, &sAIDocument,
	nullptr, 0, nullptr
};
