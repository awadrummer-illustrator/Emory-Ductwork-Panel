#include "IllustratorSDK.h"
#include "ProcessDuctworkNotes.h"
#include "ProcessDuctworkSuites.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace
{
	static std::string Trim(const std::string& value)
	{
		size_t start = 0;
		while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
			++start;
		}
		size_t end = value.size();
		while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
			--end;
		}
		return value.substr(start, end - start);
	}

	static bool ParseJsonString(const std::string& input, size_t& index, std::string& out)
	{
		if (index >= input.size() || input[index] != '"') {
			return false;
		}
		++index;
		std::ostringstream result;
		while (index < input.size()) {
			char ch = input[index++];
			if (ch == '"') {
				out = result.str();
				return true;
			}
			if (ch == '\\' && index < input.size()) {
				char esc = input[index++];
				switch (esc) {
				case '"': result << '"'; break;
				case '\\': result << '\\'; break;
				case '/': result << '/'; break;
				case 'b': result << '\b'; break;
				case 'f': result << '\f'; break;
				case 'n': result << '\n'; break;
				case 'r': result << '\r'; break;
				case 't': result << '\t'; break;
				default: result << esc; break;
				}
				continue;
			}
			result << ch;
		}
		return false;
	}

	static bool ParseJsonValue(const std::string& input, size_t& index, std::string& rawOut)
	{
		while (index < input.size() && std::isspace(static_cast<unsigned char>(input[index]))) {
			++index;
		}
		if (index >= input.size()) {
			return false;
		}
		if (input[index] == '"') {
			std::string decoded;
			size_t start = index;
			if (!ParseJsonString(input, index, decoded)) {
				return false;
			}
			rawOut = input.substr(start, index - start);
			return true;
		}

		size_t start = index;
		while (index < input.size()) {
			char ch = input[index];
			if (ch == ',' || ch == '}' || std::isspace(static_cast<unsigned char>(ch))) {
				break;
			}
			++index;
		}
		rawOut = Trim(input.substr(start, index - start));
		return !rawOut.empty();
	}
}

std::string DuctworkNotes::GetNote(AIArtHandle art)
{
	if (!art || !sAIArt) {
		return std::string();
	}

	if (!sAIArt->HasNote(art)) {
		return std::string();
	}

	ai::UnicodeString note;
	if (sAIArt->GetNote(art, note)) {
		return std::string();
	}
	return note.as_UTF8();
}

bool DuctworkNotes::SetNote(AIArtHandle art, const std::string& note)
{
	if (!art || !sAIArt) {
		return false;
	}
	ai::UnicodeString value = ai::UnicodeString::FromUTF8(note);
	return sAIArt->SetNote(art, value) == kNoErr;
}

void DuctworkNotes::ClearNote(AIArtHandle art)
{
	if (!art || !sAIArt) {
		return;
	}
	sAIArt->DeleteNote(art);
}

std::vector<std::string> DuctworkNotes::SplitTokens(const std::string& note)
{
	std::vector<std::string> tokens;
	if (note.empty()) {
		return tokens;
	}
	size_t start = 0;
	while (start <= note.size()) {
		size_t end = note.find('|', start);
		if (end == std::string::npos) {
			end = note.size();
		}
		std::string token = note.substr(start, end - start);
		if (!token.empty()) {
			tokens.push_back(token);
		}
		if (end == note.size()) {
			break;
		}
		start = end + 1;
	}
	return tokens;
}

std::string DuctworkNotes::JoinTokens(const std::vector<std::string>& tokens)
{
	std::ostringstream joined;
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i].empty()) {
			continue;
		}
		if (joined.tellp() > 0) {
			joined << "|";
		}
		joined << tokens[i];
	}
	return joined.str();
}

bool DuctworkNotes::HasToken(const std::vector<std::string>& tokens, const std::string& token)
{
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i] == token) {
			return true;
		}
	}
	return false;
}

void DuctworkNotes::AddToken(std::vector<std::string>& tokens, const std::string& token)
{
	if (token.empty() || HasToken(tokens, token)) {
		return;
	}
	tokens.push_back(token);
}

void DuctworkNotes::RemoveToken(std::vector<std::string>& tokens, const std::string& token)
{
	tokens.erase(std::remove(tokens.begin(), tokens.end(), token), tokens.end());
}

bool DuctworkNotes::ExtractMDUXMeta(const std::string& note, std::string& jsonOut, std::vector<std::string>& mdTagsOut)
{
	jsonOut.clear();
	mdTagsOut.clear();
	if (note.empty()) {
		return false;
	}
	const std::string prefix = "MDUX_META:";
	size_t start = note.find(prefix);
	if (start == std::string::npos) {
		return false;
	}
	start += prefix.size();
	size_t end = note.find('|', start);
	if (end == std::string::npos) {
		end = note.size();
	}
	jsonOut = note.substr(start, end - start);

	std::vector<std::string> tokens = SplitTokens(note);
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i].find("MD:") == 0) {
			mdTagsOut.push_back(tokens[i]);
		}
	}
	return !jsonOut.empty();
}

std::string DuctworkNotes::BuildNoteWithMDUXMeta(const std::string& json, const std::vector<std::string>& mdTags)
{
	std::string result = "MDUX_META:" + json;
	if (!mdTags.empty()) {
		result += "|";
		result += JoinTokens(mdTags);
	}
	return result;
}

bool DuctworkNotes::ParseMetaJson(const std::string& json, std::map<std::string, std::string>& out)
{
	out.clear();
	std::string trimmed = Trim(json);
	if (trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}') {
		return false;
	}
	size_t index = 1;
	while (index < trimmed.size()) {
		while (index < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[index]))) {
			++index;
		}
		if (index >= trimmed.size() || trimmed[index] == '}') {
			break;
		}
		std::string key;
		if (!ParseJsonString(trimmed, index, key)) {
			return false;
		}
		while (index < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[index]))) {
			++index;
		}
		if (index >= trimmed.size() || trimmed[index] != ':') {
			return false;
		}
		++index;
		std::string rawValue;
		if (!ParseJsonValue(trimmed, index, rawValue)) {
			return false;
		}
		out[key] = rawValue;
		while (index < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[index]))) {
			++index;
		}
		if (index < trimmed.size() && trimmed[index] == ',') {
			++index;
		}
	}
	return true;
}

std::string DuctworkNotes::SerializeMetaJson(const std::map<std::string, std::string>& fields)
{
	std::ostringstream out;
	out << "{";
	bool first = true;
	for (std::map<std::string, std::string>::const_iterator it = fields.begin(); it != fields.end(); ++it) {
		if (!first) {
			out << ",";
		}
		first = false;
		out << "\"";
		out << it->first;
		out << "\":";
		out << it->second;
	}
	out << "}";
	return out.str();
}
