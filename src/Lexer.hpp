#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <cassert>

namespace Lauter
{

	inline bool isBasicContainer(const std::string& type)
	{
		static std::unordered_set<std::string> containers = { "vector", "list", "map", "array", "set" };
		return containers.contains(type);
	}

	inline bool isPrimitiveType(const std::string& type)
	{
		static std::unordered_set<std::string> primitives = { "int", "float", "string", "bool" };
		return primitives.contains(type);
	}

	enum class TokenType : uint8_t
	{
		Integer,
		Real,
		Operator,
		String,
		Identifier,
		Keyword,

		LPAREN, //(
		LBRACE, //{
		LBRACKET, //[

		RPAREN, //)
		RBRACE, //}
		RBRACKET, //]

		COMMA,
		SEMICOLON,
		DOT,

		END
	};

	struct CharacterPos
	{
		size_t column = 1;
		size_t row = 1;
	};

	const size_t TOKEN_STRING_RESERVED_SIZE = 2048;


	struct Token
	{
		CharacterPos position;
		TokenType type;
		std::string value;

		Token(CharacterPos position, TokenType type, const std::string& value)
			: position(position), type(type), value(value) {
		}
		Token(TokenType type, const std::string& value)
			: type(type), value(value) { }
	};

	struct StreamReader
	{
	private:
		CharacterPos carriage;
		std::istream* stream = nullptr;
	public:
		void init(std::istream& stream);
		CharacterPos position() const noexcept;
		[[nodiscard]] int peek() noexcept;
		int get() noexcept;

	};

	class Lexer
	{
	private:
		std::vector<Token> extractedTokens;
		std::string tokenString;
		CharacterPos tokenStartsAt;

		StreamReader reader;

		void error(const std::string& message) const;
		void emitToken(TokenType type);

		void _readReal();
		void _readInteger();
		void _readWord();
		void _readEscapeSequence();
		void _readString();
		void _readOperator();
		void _readCommentLine();
		void _readLongComment();
		void _rootState();
	public:
		Lexer();
		std::vector<Token> extractTokens(std::istream& stream);
	};
}