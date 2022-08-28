#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace common
{

struct ParsedError
{
	std::string source_file;
	int32_t source_line = -1;
	std::string error_string;

	inline bool operator==( const ParsedError &rhs ) const
	{
		return source_file == rhs.source_file &&
			source_line == rhs.source_line &&
			error_string == rhs.error_string;
	}
};

struct ParsedErrorWithStackTrace : public ParsedError
{
	struct StackFrame
	{
		int32_t level = 0;
		std::string name;
		std::string source;
		int32_t currentline = -1;

		inline bool operator==( const StackFrame &rhs ) const
		{
			return level == rhs.level &&
				name == rhs.name &&
				source == rhs.source &&
				currentline == rhs.currentline;
		}
	};

	std::string addon_name;
	std::vector<StackFrame> stack_trace;

	inline bool operator==( const ParsedErrorWithStackTrace &rhs ) const
	{
		return source_file == rhs.source_file &&
			source_line == rhs.source_line &&
			error_string == rhs.error_string &&
			addon_name == rhs.addon_name &&
			stack_trace == rhs.stack_trace;
	}
};

bool ParseError( const std::string &error, ParsedError &parsed_error );
bool ParseErrorWithStackTrace( const std::string &error, ParsedErrorWithStackTrace &parsed_error );

}
