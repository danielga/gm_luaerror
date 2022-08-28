#include "common.hpp"

#include <cctype>
#include <cstdlib>
#include <functional>
#include <regex>
#include <sstream>

namespace common
{

inline int32_t StringToInteger( const std::string &strint )
{
	try
	{
		return std::stoi( strint );
	}
	catch( const std::exception & )
	{
		return 0;
	}
}

inline std::string Trim( const std::string &s )
{
	std::string c = s;
	auto not_isspace = std::not_fn( isspace );
	// remote trailing "spaces"
	c.erase( std::find_if( c.rbegin( ), c.rend( ), not_isspace ).base( ), c.end( ) );
	// remote initial "spaces"
	c.erase( c.begin( ), std::find_if( c.begin( ), c.end( ), not_isspace ) );
	return c;
}

bool ParseError( const std::string &error, ParsedError &parsed_error )
{
	static const std::regex error_parts_regex(
		"^(.+):(\\d+): (.+)$",
		std::regex::ECMAScript | std::regex::optimize
	);

	std::smatch matches;
	if( !std::regex_search( error, matches, error_parts_regex ) )
		return false;

	parsed_error.source_file = matches[1];
	parsed_error.source_line = StringToInteger( matches[2] );
	parsed_error.error_string = matches[3];
	return true;
}

bool ParseErrorWithStackTrace( const std::string &error, ParsedErrorWithStackTrace &parsed_error )
{
	std::istringstream error_stream( Trim( error ) );

	std::string error_first_line;
	if( !std::getline( error_stream, error_first_line ) )
		return false;

	ParsedErrorWithStackTrace temp_parsed_error;

	{
		static const std::regex client_error_addon_matcher(
			"^\\[(.+)\\] ",
			std::regex::ECMAScript | std::regex::optimize
		);

		std::smatch matches;
		if( std::regex_search( error_first_line, matches, client_error_addon_matcher ) )
		{
			temp_parsed_error.addon_name = matches[1];
			error_first_line.erase( 0, 1 + temp_parsed_error.addon_name.size( ) + 1 + 1 ); // [addon]:space:
		}
	}

	if( !ParseError( error_first_line, temp_parsed_error ) )
		temp_parsed_error.error_string = error_first_line;

	while( error_stream )
	{
		static const std::regex frame_parts_regex(
			"^\\s+(\\d+)\\. (.+) \\- (.+):(\\-?\\d+)$",
			std::regex::ECMAScript | std::regex::optimize
		);

		std::string frame_line;
		if( !std::getline( error_stream, frame_line ) )
			break;

		std::smatch matches;
		if( !std::regex_search( frame_line, matches, frame_parts_regex ) )
			return false;

		temp_parsed_error.stack_trace.emplace_back( ParsedErrorWithStackTrace::StackFrame {
			StringToInteger( matches[1] ),
			matches[2],
			matches[3],
			StringToInteger( matches[4] )
		} );
	}

	parsed_error = std::move( temp_parsed_error );
	return true;
}

}
