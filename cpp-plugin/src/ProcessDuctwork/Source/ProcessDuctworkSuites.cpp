#include "IllustratorSDK.h"
#include "ProcessDuctworkSuites.h"

extern "C" {
	SPBlocksSuite* sSPBlocks = NULL;
	AIMenuSuite* sAIMenu = NULL;
	AIUnicodeStringSuite* sAIUnicodeString = NULL;
	AIArtSuite* sAIArt = NULL;
	AIArtSetSuite* sAIArtSet = NULL;
	AIArtStyleSuite* sAIArtStyle = NULL;
	AIArtStyleParserSuite* sAIArtStyleParser = NULL;
	AIGroupSuite* sAIGroup = NULL;
	AIDictionarySuite* sAIDictionary = NULL;
	AIEntrySuite* sAIEntry = NULL;
	AILayerSuite* sAILayer = NULL;
	AIPathSuite* sAIPath = NULL;
	AIPathStyleSuite* sAIPathStyle = NULL;
	AIPaintStyleSuite* sAIPaintStyle = NULL;
	AIPlacedSuite* sAIPlaced = NULL;
	AIPanelSuite* sAIPanel = NULL;
	AILiveEditSuite* sAILiveEdit = NULL;
	AIToolSuite* sAITool = NULL;
	AIHitTestSuite* sAIHitTest = NULL;
	AITransformArtSuite* sAITransformArt = NULL;
	AIDocumentSuite* sAIDocument = NULL;
	AIDocumentViewSuite* sAIDocumentView = NULL;
	AIFileFormatSuite* sAIFileFormat = NULL;
	AIAnnotatorSuite* sAIAnnotator = NULL;
	AIAnnotatorDrawerSuite* sAIAnnotatorDrawer = NULL;
	AICSXSExtensionSuite* sAICSXSExtension = NULL;
	AIUndoSuite* sAIUndo = NULL;
}

ImportSuite gImportSuites[] =
{
	kSPBlocksSuite, kSPBlocksSuiteVersion, &sSPBlocks,
	kAIMenuSuite, kAIMenuSuiteVersion, &sAIMenu,
	kAIUnicodeStringSuite, kAIUnicodeStringVersion, &sAIUnicodeString,
	kAIArtSuite, kAIArtSuiteVersion, &sAIArt,
	kAIArtSetSuite, kAIArtSetSuiteVersion, &sAIArtSet,
	kAIArtStyleSuite, kAIArtStyleSuiteVersion, &sAIArtStyle,
	kAIArtStyleParserSuite, kAIArtStyleParserSuiteVersion, &sAIArtStyleParser,
	kAIGroupSuite, kAIGroupSuiteVersion, &sAIGroup,
	kAIDictionarySuite, kAIDictionarySuiteVersion, &sAIDictionary,
	kAIEntrySuite, kAIEntrySuiteVersion, &sAIEntry,
	kAILayerSuite, kAILayerSuiteVersion, &sAILayer,
	kAIPathSuite, kAIPathSuiteVersion, &sAIPath,
	kAIPathStyleSuite, kAIPathStyleSuiteVersion, &sAIPathStyle,
	kAIPaintStyleSuite, kAIPaintStyleSuiteVersion, &sAIPaintStyle,
	kAIPlacedSuite, kAIPlacedSuiteVersion, &sAIPlaced,
	kAIPanelSuite, kAIPanelSuiteVersion, &sAIPanel,
	kAILiveEditSuite, kAILiveEditSuiteVersion, &sAILiveEdit,
	kAIToolSuite, kAIToolSuiteVersion, &sAITool,
	kAIHitTestSuite, kAIHitTestSuiteVersion, &sAIHitTest,
	kAITransformArtSuite, kAITransformArtSuiteVersion, &sAITransformArt,
	kAIDocumentSuite, kAIDocumentSuiteVersion, &sAIDocument,
	kAIDocumentViewSuite, kAIDocumentViewSuiteVersion, &sAIDocumentView,
	kAIFileFormatSuite, kAIFileFormatSuiteVersion, &sAIFileFormat,
	kAIAnnotatorSuite, kAIAnnotatorSuiteVersion, &sAIAnnotator,
	kAIAnnotatorDrawerSuite, kAIAnnotatorDrawerSuiteVersion, &sAIAnnotatorDrawer,
	kAICSXSExtensionSuite, kAICSXSExtensionSuiteVersion, &sAICSXSExtension,
	kAIUndoSuite, kAIUndoSuiteVersion, &sAIUndo,
	nullptr, 0, nullptr
};
