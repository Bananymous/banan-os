#pragma once

#include <BAN/Assert.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>

struct Token
{
public:
	enum class Type
	{
		EOF_,

		Ampersand,
		Backslash,
		CloseCurly,
		CloseParen,
		Dollar,
		DoubleQuote,
		GreaterThan,
		LessThan,
		OpenCurly,
		OpenParen,
		Pipe,
		Semicolon,
		SingleQuote,
		String,
		Whitespace,
	};

	Token(Type type)
		: m_type(type)
	{}

	Token(Type type, BAN::String&& string)
		: m_type(type)
	{
		ASSERT(type == Type::String || type == Type::Whitespace);
		if (type == Type::Whitespace)
			for (char c : string)
				ASSERT(isspace(c));
		m_value = BAN::move(string);
	}

	Token(Token&& other)
	{
		m_type = other.m_type;
		m_value = other.m_value;
		other.clear();
	}

	Token& operator=(Token&& other)
	{
		m_type = other.m_type;
		m_value = other.m_value;
		other.clear();
		return *this;
	}

	Token(const Token&) = delete;
	Token& operator=(const Token&) = delete;

	~Token()
	{
		clear();
	}

	Type type() const { return m_type; }

	      BAN::String& string()       { ASSERT(m_type == Type::String || m_type == Type::Whitespace); return m_value; }
	const BAN::String& string() const { ASSERT(m_type == Type::String || m_type == Type::Whitespace); return m_value; }

	void clear()
	{
		m_type = Type::EOF_;
		m_value.clear();
	}

	void debug_dump() const;

private:
	Type m_type { Type::EOF_ };
	BAN::String m_value;
};
