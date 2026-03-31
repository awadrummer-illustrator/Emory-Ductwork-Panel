#ifndef __ProcessDuctworkNotes_H__
#define __ProcessDuctworkNotes_H__

#include "IllustratorSDK.h"

#include <map>
#include <string>
#include <vector>

namespace DuctworkNotes
{
	std::string GetNote(AIArtHandle art);
	bool SetNote(AIArtHandle art, const std::string& note);
	void ClearNote(AIArtHandle art);

	std::vector<std::string> SplitTokens(const std::string& note);
	std::string JoinTokens(const std::vector<std::string>& tokens);
	bool HasToken(const std::vector<std::string>& tokens, const std::string& token);
	void AddToken(std::vector<std::string>& tokens, const std::string& token);
	void RemoveToken(std::vector<std::string>& tokens, const std::string& token);

	bool ExtractMDUXMeta(const std::string& note, std::string& jsonOut, std::vector<std::string>& mdTagsOut);
	std::string BuildNoteWithMDUXMeta(const std::string& json, const std::vector<std::string>& mdTags);

	bool ParseMetaJson(const std::string& json, std::map<std::string, std::string>& out);
	std::string SerializeMetaJson(const std::map<std::string, std::string>& fields);
}

#endif // __ProcessDuctworkNotes_H__
