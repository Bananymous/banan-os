#include "Token.h"

#include <BAN/Debug.h>

void Token::debug_dump() const
{
	switch (type())
	{
		case Type::EOF_:
			dwarnln("Token <EOF>");
			break;
		case Type::Ampersand:
			dprintln("Token <Ampersand>");
			break;
		case Type::Backslash:
			dprintln("Token <Backslash>");
			break;
		case Type::CloseCurly:
			dprintln("Token <CloseCurly>");
			break;
		case Type::CloseParen:
			dprintln("Token <CloseParen>");
			break;
		case Type::Dollar:
			dprintln("Token <Dollar>");
			break;
		case Type::DoubleQuote:
			dprintln("Token <DoubleQuote>");
			break;
		case Type::GreaterThan:
			dprintln("Token <GreaterThan>");
			break;
		case Type::LessThan:
			dprintln("Token <LessThan>");
			break;
		case Type::OpenCurly:
			dprintln("Token <OpenCurly>");
			break;
		case Type::OpenParen:
			dprintln("Token <OpenParen>");
			break;
		case Type::Pipe:
			dprintln("Token <Pipe>");
			break;
		case Type::Semicolon:
			dprintln("Token <Semicolon>");
			break;
		case Type::SingleQuote:
			dprintln("Token <SingleQuote>");
			break;
		case Type::String:
			dprintln("Token <String \"{}\">", string());
			break;
		case Type::Whitespace:
			dprintln("Token <Whitespace \"{}\">", string());
			break;
	}
}
