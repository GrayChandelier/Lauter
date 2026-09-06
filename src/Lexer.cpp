#include "Lexer.hpp"

namespace Lauter
{
	namespace
	{
		inline bool isArithmeticalOperator(char character) noexcept
		{
			switch (character)
			{
			case '+': return true;
			case '-': return true;
			case '*': return true;
			case '/': return true;
			case '%': return true;
			default: return false;
			}
		}

		inline bool isBitwiseOperator(char character) noexcept
		{
			switch (character)
			{
			case '!': return true;
			case '^': return true;
			case '|': return true;
			case '&': return true;
			case '~': return true;
			default: return false;
			}
		}

		inline bool isBooleanOperator(const std::string& token) noexcept
		{
			if (token == "and") return true;
			if (token == "or") return true;
			if (token == "!") return true;

			return false;
		}

		inline bool isRelationOperator(const std::string& token) noexcept
		{
			if (token == ">") return true;
			if (token == "<") return true;

			if (token == "==") return true;
			if (token == "!=") return true;

			if (token == "<=") return true;
			if (token == ">=") return true;

			//absolute equal (same object in memory)
			if (token == "===") return true;

			return false;
		}

		inline bool isTokenBoundary(char c)
		{
			return std::isspace((unsigned char)c)
				|| c == '(' || c == ')'
				|| c == '{' || c == '}'
				|| c == '[' || c == ']'
				|| c == ',' || c == ';' || c == '.'
				|| c == '+' || c == '-' || c == '*'
				|| c == '/' || c == '='
				|| c == '<' || c == '>'
				|| c == '!' || c == '&' || c == '|'
				|| c == '^' || c == '~' 
				|| c == ':';
		}

		inline bool isKeyword(const std::string& token) noexcept
		{
			static std::unordered_set<std::string> keywords = {
				"def", "for", "switch", "return",  "if", "else",
				"const", "true", "false", "void", 
				"when", "while","ref", "in", "at", "with",
				"public", "private", "protected", "constructor", "destructor",
				"import", "interface","class", "as", "cascade"
			};
			return keywords.contains(token);
		}
		inline bool isOperator(const std::string& token) noexcept
		{
			static std::unordered_set<std::string> operators = {
				"and", "or", "not"
			};
			return operators.contains(token);
		}
	}

	void StreamReader::init(std::istream& stream)
	{
		this->stream = &stream;
		carriage = CharacterPos{ 1,1 };

	}
	CharacterPos StreamReader::position() const noexcept
	{
		return carriage;
	}

	[[nodiscard]] int StreamReader::peek() noexcept
	{
		assert(stream);
		return stream->peek();
	}
	int StreamReader::get() noexcept
	{
		assert(stream);

		int character = stream->get();

		if (character == '\r')
		{
			carriage.column = 1;
		}
		else if (character == '\n')
		{
			carriage.row++;
			carriage.column = 1;
		}
		else carriage.column++;

		return character;
	}


	void Lexer::error(const std::string& message) const
	{
		std::cerr << message << std::endl;
	}

	void Lexer::emitToken(TokenType type)
	{
		extractedTokens.push_back(Token{ tokenStartsAt, type, tokenString });
		tokenString.clear();
	}

	void Lexer::_readReal()
	{
		int character;
		while ((character = reader.peek()) != EOF)
		{
			if (character == '.')
			{
				error("Real number already has dot");
				return;
			}

			if (!isdigit(character))
				break;

			tokenString += character;
			reader.get();
		}

		emitToken(TokenType::Real);
	}
	void Lexer::_readInteger()
	{
		tokenStartsAt = reader.position();

		int character;
		while ((character = reader.peek()) != EOF)
		{
			if (character == '.')
			{
				tokenString += static_cast<char>(reader.get());
				_readReal();
				return;
			}

			if (!isdigit(character))
				break;

			tokenString += character;
			reader.get();
		}

		emitToken(TokenType::Integer);
	}

	void Lexer::_readWord()
	{
		tokenStartsAt = reader.position();

		int character;
		while ((character = reader.peek()) != EOF)
		{
			if (isTokenBoundary(character))
				break;

			tokenString += character;
			reader.get();
		}

		if (isKeyword(tokenString))
			emitToken(TokenType::Keyword);
		else if(isOperator(tokenString))
			emitToken(TokenType::Operator);
		else
			emitToken(TokenType::Identifier);
	}

	void Lexer::_readEscapeSequence()
	{
		switch (int character = reader.get())
		{
		case 'a': tokenString += '\a'; break;
		case 'n': tokenString += '\n'; break;
		case 'v': tokenString += '\v'; break;
		case 't': tokenString += '\t'; break;
		case 'r': tokenString += '\r'; break;
		default: tokenString += static_cast<char>(character);
		}
	}
	void Lexer::_readString()
	{
		tokenStartsAt = reader.position();

		reader.get(); //eat first "
		int character;
		while ((character = reader.peek()) != EOF)
		{
			if (character == '\\')
			{
				reader.get(); //eat '\'
				_readEscapeSequence();
				continue;
			}

			if (character == '\"')
				break;

			tokenString += character;
			reader.get();
		}
		reader.get(); // eat last "
		emitToken(TokenType::String);
	}

	void Lexer::_readOperator()
	{

		static std::unordered_set<std::string> arithOps = {
				"+", "-", "*", "/", "%", "**",
				"++", "--",
				"+=", "-=", "*=", "/=", "%=", "**="
		};

		static std::unordered_set<std::string> relOps = {
			">", "<",
			"==", "!=", "===",
			">=", "<="
		};

		static std::unordered_set<std::string> bitwiseOps = {
			"~", "^", "|", "&"
		};


		static std::unordered_set<std::string> specificOps = 
		{ "=>", "::", ":"};


		int character;
		while ((character = reader.peek()) != EOF)
		{
			std::string extension = tokenString + static_cast<char>(character);

			if (!arithOps.contains(extension) &&
				!relOps.contains(extension) &&
				!bitwiseOps.contains(extension) &&
				!specificOps.contains(extension))
				break;

			tokenString = extension;
			reader.get();
		}

		emitToken(TokenType::Operator);
	}

	void Lexer::_readCommentLine()
	{
		int character;
		while ((character = reader.get()) != EOF && character != '\n') {}
	}

	void Lexer::_readLongComment()
	{
		int last = -1;
		int character;
		while ((character = reader.peek()) != EOF)
		{
			//use */ to end comment
			if (last == '*' && character == '/')
			{
				reader.get();
				return;
			}

			last = character;
			reader.get();
		}
	}

	void Lexer::_rootState()
	{
		int character;
		while ((character = reader.peek()) != EOF)
		{
			if (isspace(character))
			{
				reader.get(); continue;
			}
			else if (isdigit(character))
				_readInteger();
			else if (character == '"')
				_readString();
			else if (character == ',')
			{
				tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get()); emitToken(TokenType::COMMA);
			}
			else if (character == ';')
			{
				tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get()); emitToken(TokenType::SEMICOLON);
			}
			else if (character == '.')
			{
				tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get()); emitToken(TokenType::DOT);
			}
			else if (!isTokenBoundary(character))
				_readWord();
			else if (character == '/')
			{
				tokenStartsAt = reader.position();
				reader.get(); //eat '/'

				char next = reader.peek();

				if (next == '/')
				{
					reader.get();
					_readCommentLine();
				}
				else if (next == '*')
				{
					reader.get();
					_readLongComment();
				}
				else
				{
					tokenString = character;
					_readOperator();
				}
			}

			else if (character == '(') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::LPAREN); }
			else if (character == ')') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::RPAREN); }
			else if (character == '{') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::LBRACE); }
			else if (character == '}') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::RBRACE); }
			else if (character == '[') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::LBRACKET); }
			else if (character == ']') { tokenStartsAt = reader.position(); tokenString = static_cast<char>(reader.get());  emitToken(TokenType::RBRACKET); }
			else
			{
				tokenStartsAt = reader.position();
				tokenString += static_cast<char>(reader.get());
				_readOperator();
			}

		}
	}

	Lexer::Lexer()
	{
		tokenString.reserve(TOKEN_STRING_RESERVED_SIZE);
	}
	std::vector<Token> Lexer::extractTokens(std::istream& stream)
	{
		extractedTokens.clear();
		reader.init(stream);

		_rootState();

		std::vector<Token> result{};
		std::swap(result, extractedTokens);

		static Token end(CharacterPos{0,0}, TokenType::END, "");
		result.push_back(end);

		return result;
	}
}