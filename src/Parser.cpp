#include "Parser.hpp"

namespace Lauter
{
	using namespace AST;

	SourceLocation getLocation(const Token& token) noexcept
	{
		return SourceLocation{ .row = token.position.row, .column = token.position.column };
	}

	ReportLocation Parser::getReportLocation(const Token& token) const
	{ 
		return ReportLocation{ .source = "",  .column = token.position.column, .row = token.position.row, };
	}

	[[nodiscard]] constexpr std::string_view to_string(TokenType type) noexcept
	{
		switch (type)
		{
		case TokenType::Integer:    return "Integer";
		case TokenType::Real:       return "Real";
		case TokenType::Operator:   return "Operator";
		case TokenType::String:     return "String";
		case TokenType::Identifier: return "Identifier";
		case TokenType::Keyword:    return "Keyword";

		case TokenType::LPAREN:     return "LPAREN";
		case TokenType::LBRACE:     return "LBRACE";
		case TokenType::LBRACKET:   return "LBRACKET";

		case TokenType::RPAREN:     return "RPAREN";
		case TokenType::RBRACE:     return "RBRACE";
		case TokenType::RBRACKET:   return "RBRACKET";

		case TokenType::COMMA:      return "COMMA";
		case TokenType::SEMICOLON:  return "SEMICOLON";
		case TokenType::DOT:        return "DOT";

		case TokenType::END:        return "END";

			// На случай, если в функцию прилетит невалидный каст, например: static_cast<TokenType>(99)
		default:                    return "Unknown";
		}
	}

	bool TokenStreamReader::end() const noexcept
	{
		return current >= tokenStream.size();
	}

	const Token& TokenStreamReader::peek(size_t offset) const noexcept
	{
		size_t idx = current + offset;
		if (idx >= tokenStream.size())
			return tokenStream[tokenStream.size() - 1];

		return tokenStream[idx];
	}

	Token TokenStreamReader::get() noexcept
	{
		Token token = peek();
		++current;
		return token;
	}

	[[noreturn]] void Parser::unexpectedToken(const Token& token, TokenType expectedType) const
	{
		throw ParseError(
			getLocation(token),
			"unexpected token '" + token.value + "', expects type <" + std::string(to_string(expectedType)) + ">"
		);
	}


	[[noreturn]] void Parser::unexpectedToken(const Token& token) const
	{
		throw ParseError(
			getLocation(token),
			"unexpected token '" + token.value + "'"
		);
	}


	bool Parser::check(TokenType type, const std::string& value) const
	{
		const Token& token = stream.peek();

		//Проверка на совпадение типа
		if (token.type != type)
			return false;

		//Проверка на совпадение содержимого
		if (!value.empty() && token.value != value)
			return false;

		return true;
	}
	bool Parser::match(TokenType type, const std::string& value)
	{
		if (!check(type, value))
			return false;

		stream.get();
		return true;
	}
	Token Parser::expect(TokenType type, const std::string& value)
	{
		const Token& token = stream.peek();

		if (token.type != type)
			unexpectedToken(token);

		if (!value.empty() && token.value != value)
			unexpectedToken(token);

		return stream.get();
	}
	bool Parser::isStatementStart() const
	{
		return
			check(TokenType::Keyword, "if") ||
			check(TokenType::Keyword, "while") ||
			check(TokenType::Keyword, "for") ||
			check(TokenType::Keyword, "when") ||
			check(TokenType::Keyword, "return") ||
			check(TokenType::Keyword, "break") ||
			check(TokenType::Keyword, "continue") ||
			check(TokenType::Identifier);
	}

	bool Parser::isTopLevelStart() const
	{
		return
			check(TokenType::Keyword, "import") ||
			check(TokenType::Keyword, "namespace") ||
			check(TokenType::Keyword, "class") ||
			check(TokenType::Keyword, "interface") ||
			check(TokenType::Keyword, "def") ||
			check(TokenType::Keyword, "at");
	}

	//Съедает невалидный участок кода
	void Parser::synchronize()
	{
		while (!stream.end())
		{
		

			if (match(TokenType::RBRACE) || isStatementStart())
				break;	

			stream.get();
		}
	}

	void Parser::synchronizeTopLevel()
	{
		while (!stream.end())
		{
			

			if (match(TokenType::RBRACE) || isTopLevelStart())
				break;

			stream.get();
		}
	}

	//Заглушка для recovery
	AST::ExpressionPtr Parser::makeErrorExpression(SourceLocation location) const
	{
		return std::make_unique<ErrorExpression>(location);
	}
	//Заглушка для recovery
	AST::StatementPtr Parser::makeErrorStatement(SourceLocation location) const
	{
		return std::make_unique<ErrorStatement>(location);
	}
	//Заглушка для recovery
	AST::DeclarationPtr Parser::makeErrorDeclaration(SourceLocation location) const
	{
		return std::make_unique<ErrorDeclaration>(location);
	}

	AST::StatementPtr Parser::parseStatementSafely()
	{
		try
		{
			return parseStatement();
		}
		catch (const ParseError& ex)
		{
			Report report;
			report.location.source = "<?>";//заглушка
			report.location.row = ex.location.row;
			report.location.column = ex.location.column;
			report.message = ex.message;
			report.severity = Severity::Error;
			report.code = ReportCode::ParsingError;

			diagnostics.report(std::move(report));

			synchronize();
			return makeErrorStatement(ex.location);
		}
	}
	AST::DeclarationPtr Parser::parseTopLevelDeclarationSafely()
	{
		try
		{
			return parseTopLevelDeclaration();
		}
		catch (const ParseError& ex)
		{
			Report report;
			report.location.source = "<?>";//заглушка
			report.location.row = ex.location.row;
			report.location.column = ex.location.column;
			report.message = ex.message;
			report.severity = Severity::Error;
			report.code = ReportCode::ParsingError;

			diagnostics.report(std::move(report));

			synchronizeTopLevel();
			return makeErrorDeclaration(ex.location);
		}
	}

	//Собирает составные имена вида A или A::B::C и т.п.
	QualifiedName Parser::parseQualifiedName()
	{
		QualifiedName name;

		name.parts.push_back(expect(TokenType::Identifier).value);

		//Продолжает цепочку вложенных пространств имён
		while (match(TokenType::Operator, "::"))
		{
			name.parts.push_back(expect(TokenType::Identifier).value);
		}

		return name;
	}

	//Собирает квалификаторы, имя объекта, круглые скобки и шаблонные параметры
	TypeRef Parser::parseTypeRef()
	{
		TypeRef type;

		type.isConst = match(TokenType::Keyword, "const");
		type.isReference = match(TokenType::Keyword, "ref");

		Token token = stream.peek();

		if (token.type != TokenType::Identifier)
			unexpectedToken(token, TokenType::Identifier);

		type.location = getLocation(token);

		type.typeName = parseQualifiedName();

		//Обработка дженериков
		if (match(TokenType::LPAREN))
		{
			type.generics = parseGenericArgumentList();
			expect(TokenType::RPAREN);
		}

		return type;
	}


	GenericArgument Parser::parseGenericArgument()
	{
		GenericArgument arg;

		const Token& token = stream.peek();
		arg.location = getLocation(token);

		switch (token.type)
		{
		case TokenType::Identifier:
		{
			arg.value = std::make_unique<TypeRef>(parseTypeRef());
			break;
		}

		case TokenType::Integer:
		{
			LiteralValue value = static_cast<int64_t>(
				std::stoll(stream.get().value)
				);

			arg.value = value;
			break;
		}

		case TokenType::Real:
		{
			LiteralValue value = static_cast<double>(
				std::stod(stream.get().value)
				);

			arg.value = value;
			break;
		}

		case TokenType::String:
		{
			arg.value = stream.get().value;
			break;
		}

		case TokenType::Keyword:
		{
			if (token.value == "true" || token.value == "false")
			{
				arg.value = token.value == "true";
				stream.get();
				break;
			}

			[[fallthrough]];
		}

		default:
			unexpectedToken(token);
		}

		return arg;
	}

	//Собирает шаблонные аргументы и запятые между ними
	std::vector<GenericArgument> Parser::parseGenericArgumentList()
	{
		std::vector<GenericArgument> result;

		if (check(TokenType::RPAREN))
			return result;

		do
		{
			result.push_back(parseGenericArgument());
		} while (match(TokenType::COMMA));

		return result;
	}

	//Собирает TypeRef, имя параметра и значение по умолчанию
	Parameter Parser::parseParameter()
	{

		TypeRef typeref = parseTypeRef();
		Token token = expect(TokenType::Identifier);

		Parameter param(getLocation(token));
		param.name = token.value;
		param.type = std::move(typeref);

		if (match(TokenType::Operator, "="))
			param.defaultValue = parseExpression();

		return param;
	}

	//Собирает параметры и запятые между ними
	std::vector<Parameter> Parser::parseParameterList()
	{
		expect(TokenType::LPAREN); //'('

		std::vector<Parameter> result;

		if (match(TokenType::RPAREN))
			return result;

		bool defaultParamChainStarted = false;

		do
		{
			Parameter param = parseParameter();

			bool currentHasDefaultParam = param.defaultValue != nullptr;

			if (defaultParamChainStarted && !currentHasDefaultParam)
			{
				throw ParseError(
					param.getLocation(),
					"non-default parameter cannot follow default parameter"
				);
			}

			defaultParamChainStarted |= currentHasDefaultParam;

			result.push_back(std::move(param));

		} while (match(TokenType::COMMA));

		expect(TokenType::RPAREN); //')'

		return result;
	}

	//Собирает TypeRef и запятые между ними
	std::vector<TypeRef>  Parser::parseReturnTypeList()
	{
		std::vector<TypeRef> returns;

		do
		{
			returns.push_back(parseTypeRef());

		} while (match(TokenType::COMMA));

		return returns;
	}



	//РАЗБОР ВЫРАЖЕНИЙ

	AST::ExpressionPtr Parser::parseExpression()
	{
		return parseAssignment();
	}

	AST::ExpressionPtr Parser::parseAssignment()
	{
		auto left = parseOr();

		//Если не встретился оператор присваивания — возвращаем уже разобранное выражение
		if (!check(TokenType::Operator))
			return left;

		const auto& token = stream.peek();
		AssignmentOperator op;

		if (token.value == "=")
			op = AssignmentOperator::Assign;
		else if (token.value == "+=")
			op = AssignmentOperator::AddAssign;
		else if (token.value == "-=")
			op = AssignmentOperator::SubAssign;
		else if (token.value == "*=")
			op = AssignmentOperator::MulAssign;
		else if (token.value == "/=")
			op = AssignmentOperator::DivAssign;
		else if (token.value == "%=")
			op = AssignmentOperator::ModAssign;
		else
			return left;

		//Съедаем оператор присваивания
		stream.get();

		auto node = std::make_unique<AssignmentExpression>();

		node->op = op;
		node->target = std::move(left);

		//Разбираем выражение после оператора присваивания
		node->value = parseAssignment();

		return node;
	}

	ExpressionPtr Parser::parseOr()
	{
		auto left = parseAnd();

		//Разбираем цепочку дизъюнкций
		while (match(TokenType::Operator, "or"))
		{
			auto node = std::make_unique<BinaryExpression>();

			node->op = BinaryOperator::LogicalOr;
			node->leftOperand = std::move(left);
			node->rightOperand = parseAnd();

			left = std::move(node);
		}

		return left;
	}

	ExpressionPtr Parser::parseAnd()
	{
		auto left = parseComparison();

		//Разбираем цепочку конъюнкций
		while (match(TokenType::Operator, "and"))
		{
			auto node = std::make_unique<BinaryExpression>();

			node->op = BinaryOperator::LogicalAnd;
			node->leftOperand = std::move(left);
			node->rightOperand = parseComparison();

			left = std::move(node);
		}

		return left;
	}

	ExpressionPtr Parser::parseComparison()
	{
		auto left = parseAddSub();

		BinaryOperator op;
		const auto& token = stream.peek();

		if (token.value == "==")
			op = BinaryOperator::Equal;
		else if (token.value == "!=")
			op = BinaryOperator::NotEqual;
		else if (token.value == "<")
			op = BinaryOperator::Less;
		else if (token.value == "<=")
			op = BinaryOperator::LessEqual;
		else if (token.value == ">")
			op = BinaryOperator::Greater;
		else if (token.value == ">=")
			op = BinaryOperator::GreaterEqual;
		else
			//Если оператор не является оператором сравнения
			return left;

		stream.get();

		auto node = std::make_unique<BinaryExpression>();

		node->op = op;
		node->leftOperand = std::move(left);
		node->rightOperand = parseAddSub();

		left = std::move(node);


		return left;
	}

	ExpressionPtr Parser::parseAddSub()
	{
		auto left = parseMulDiv();

		//Разбираем арифметические операции сложения и вычитания
		while (check(TokenType::Operator))
		{
			BinaryOperator op;

			if (match(TokenType::Operator, "+"))
				op = BinaryOperator::Add;
			else if (match(TokenType::Operator, "-"))
				op = BinaryOperator::Sub;
			else
				break;

			auto node = std::make_unique<BinaryExpression>();

			node->op = op;
			node->leftOperand = std::move(left);
			node->rightOperand = parseMulDiv();

			left = std::move(node);
		}

		return left;
	}

	ExpressionPtr Parser::parseMulDiv()
	{
		auto left = parseUnary();

		//Разбираем арифметические операции умножения и деления, а также деления по модулю
		while (check(TokenType::Operator))
		{
			BinaryOperator op;

			if (match(TokenType::Operator, "*"))
				op = BinaryOperator::Mul;
			else if (match(TokenType::Operator, "/"))
				op = BinaryOperator::Div;
			else if (match(TokenType::Operator, "%"))
				op = BinaryOperator::Mod;
			else
				break;

			auto node = std::make_unique<BinaryExpression>();

			node->op = op;
			node->leftOperand = std::move(left);
			node->rightOperand = parseUnary();

			left = std::move(node);
		}

		return left;
	}


	AST::ExpressionPtr Parser::parsePower()
	{
		auto left = parsePostfix();

		//Разбор правоассоциативной операции возведения в степень
		if (!match(TokenType::Operator, "**"))
			return left;

		auto node = std::make_unique<BinaryExpression>();

		node->op = BinaryOperator::Power;
		node->leftOperand = std::move(left);

		//Правоассоциативность
		node->rightOperand = parsePower();

		return node;
	}

	ExpressionPtr Parser::parseUnary()
	{
		//Разбор унарных операций
		if (check(TokenType::Operator))
		{
			UnaryOperator op;

			if (match(TokenType::Operator, "-"))
				op = UnaryOperator::Negate;
			else if (match(TokenType::Operator, "+"))
				op = UnaryOperator::Positive;
			else if (match(TokenType::Operator, "not"))
				op = UnaryOperator::LogicalNot;
			else if (match(TokenType::Operator, "~"))
				op = UnaryOperator::BitwiseNot;
			else
				unexpectedToken(stream.peek());


			auto node = std::make_unique<UnaryExpression>();

			node->op = op;
			node->operand = parsePower();

			return node;
		}

		return parsePower();
	}

	//Постфиксные выражения (оператор индекса, обращение к методам и полям)
	ExpressionPtr Parser::parsePostfix()
	{
		//identifier, literal, (expr), tuple и т.п.
		auto target = parsePrimary();

		//Список операций (в том числе операции над результатом — "arr[index](argument)()" )
		std::vector<PostfixOperation> operations;

		while (true)
		{
			//Разбор обращения по индексу
			if (match(TokenType::LBRACKET))
			{
				IndexOperation op;

				//выражение внутри [...]
				op.index = parseExpression();

				expect(TokenType::RBRACKET);

				operations.push_back(std::move(op));
				continue;
			}

			//Разбор обращения к полю объекта
			if (match(TokenType::DOT))
			{
				const Token& id = expect(TokenType::Identifier);

				MemberAccessOperation op;
				op.member = id.value;

				operations.push_back(std::move(op));
				continue;
			}

			//Вызов метода/результата 
			if (match(TokenType::LPAREN))
			{
				CallOperation op;

				op.arguments = parseArgList();

				expect(TokenType::RPAREN);

				operations.push_back(std::move(op));
				continue;
			}

			//Если следующая операция не является постфиксной — разбор окончен
			break;
		}

		//Если список операций пуст — не создаём лишний узел AST
		if (operations.empty())
			return target;

		//Иначе создаём PostfixExpression
		auto node = std::make_unique<PostfixExpression>();

		node->target = std::move(target);
		node->operations = std::move(operations);

		return node;
	}

	AST::ExpressionPtr Parser::parsePrimary()
	{
		const Token& token = stream.peek();

		switch (token.type)
		{
			//Целочисленный литерал
		case TokenType::Integer:
		{
			auto node = std::make_unique<AST::Literal>();

			node->kind = LiteralType::Int;
			node->value = static_cast<int64_t>(
				std::stoll(stream.get().value)
				);

			return node;
		}

		//Литерал вещественного числа
		case TokenType::Real:
		{
			auto node = std::make_unique<AST::Literal>();

			node->kind = LiteralType::Real;
			node->value = std::stod(stream.get().value);

			return node;
		}

		//Строковой литерал
		case TokenType::String:
		{
			auto node = std::make_unique<AST::Literal>();

			node->kind = LiteralType::String;
			node->value = stream.get().value;

			return node;
		}

		//Логическая истина или логическая ложь как ключевое слово
		case TokenType::Keyword:
		{
			if (token.value == "true" || token.value == "false")
			{
				auto node = std::make_unique<AST::Literal>();

				node->kind = LiteralType::Boolean;
				node->value = (token.value == "true");

				stream.get();

				return node;
			}

			break;
		}


		//Объект
		case TokenType::Identifier:
		{
			auto node = std::make_unique<AST::IdentifierExpression>(
				getLocation(token),
				parseQualifiedName()
			);

			return node;
		}

		//Литерал контейнера
		case TokenType::LBRACKET:
		{
			return parseContainerLiteralExpression();
		}

		case TokenType::LPAREN: //FIX
		{
			expect(TokenType::LPAREN);
			auto node = parseExpression();
			expect(TokenType::RPAREN);
			return node;
		}

		default:
			break;
		}


		unexpectedToken(token);
		return makeErrorExpression(getLocation(token));
	}

	//Проверяет, является ли выражение началом цепочки неоднозначных вызовов
	//Исключает вероятность попадания выражений вида A + B в AmbiguousChain
	bool Parser::isAmbiguousConstruct() const
	{
		if (!check(TokenType::Identifier))
			return false;

		size_t offset = 0;

		offset++;

		while (stream.peek(offset).type == TokenType::Operator &&
			stream.peek(offset).value == "::")
		{
			offset++;

			if (stream.peek(offset).type != TokenType::Identifier)
				return false;

			offset++;
		}


		if (stream.peek(offset).type == TokenType::LPAREN)
		{
			int depth = 1;
			offset++;

			while (depth > 0)
			{
				auto type = stream.peek(offset).type;

				if (type == TokenType::END)
					return false;

				if (type == TokenType::LPAREN)
					depth++;

				else if (type == TokenType::RPAREN)
					depth--;

				offset++;
			}
		}


		TokenType next = stream.peek(offset).type;


		// IDENT IDENT
		if (next == TokenType::Identifier)
			return true;


		// IDENT keyword
		if (next == TokenType::Keyword)
			return true;


		return false;
	}

	//Собирает токены в цепочку неоднозначных вызовов для дальншего разбора в семантическом анализаторе
	AST::StatementPtr Parser::parseAmbiguousStatement()
	{
		auto chain = std::make_unique<AST::AmbiguousChain>();

		while (true)
		{
			Token identName = stream.peek();
			QualifiedName name = parseQualifiedName();
			AST::AmbiguousChain::Segment segment(getLocation(identName), name);
			if (match(TokenType::LPAREN))
			{
				segment.arguments = parseArgList();
				expect(TokenType::RPAREN);
			}
			chain->segments.push_back(std::move(segment));

			if (!check(TokenType::Identifier))
				break; // впереди не IDENT вовсе — цепочка кончилась раньше нашего lookahead'а

			TokenType nextType = stream.peek(1).type;

			// IDENT IDENT
			if (nextType == TokenType::Identifier)
				continue;

			// IDENT (
			if (nextType == TokenType::LPAREN)
				continue;

			// IDENT keyword — часть цепочки, цепочка завершается на нём
			if (nextType == TokenType::Keyword)
			{
				Token identTok = stream.peek();
				QualifiedName trailingName = parseQualifiedName();
				chain->segments.emplace_back(getLocation(identTok), trailingName);
				break;
			}

			// IDENT = — последний сегмент цепочки + инициализатор
			if (nextType == TokenType::Operator && stream.peek(1).value == "=")
			{
				Token identTok = stream.peek();
				QualifiedName trailingName = parseQualifiedName();
				chain->segments.emplace_back(getLocation(identTok), trailingName);

				expect(TokenType::Operator, "=");
				chain->lastSegmentInitializer = parseExpression();
				break;
			}

			// IDENT operator (любой другой) — не часть цепочки, оставляем непотреблённым
			break;
		}

		return chain;
	}

	AST::StatementPtr Parser::parseBlock()
	{
		expect(TokenType::LBRACE);
		auto node = std::make_unique<AST::Block>();

		//Сбор списка инструкций до }
		while (!match(TokenType::RBRACE))
		{
			auto stmt = parseStatement();
			node->statements.push_back(std::move(stmt));

			if (check(TokenType::END))
				break;
		}

		return node;
	}
	AST::Body Parser::parseBody()
	{
		if (!check(TokenType::LBRACE))
			return parseStatement();

		return parseBlock();
	}

	AST::StatementPtr Parser::parseIfStatement()
	{
		auto node = std::make_unique<AST::IfStatement>();

		expect(TokenType::Keyword, "if");

		//Условие
		node->condition = parseExpression();

		//then-блок
		node->thenBody = parseBody();

		//else-блок
		if (match(TokenType::Keyword, "else"))
			node->elseBody = parseBody();

		return node;
	}
	AST::StatementPtr Parser::parseWhileStatement()
	{
		auto node = std::make_unique<AST::WhileStatement>();

		expect(TokenType::Keyword, "while");
		
		//RAII защита от вызова continue/break вне тела цикла
		LoopGuard loopGuard(loopDepth);

		//Условие
		node->condition = parseExpression();

		//Тело цикла
		node->body = parseBlock(); //Не допускает отсутствия фигурных скобок


		return node;
	}

	AST::StatementPtr Parser::parseForStatement()
	{
		auto node = std::make_unique<AST::ForInStatement>();

		expect(TokenType::Keyword, "for");

		//RAII защита от вызова continue/break вне тела цикла
		LoopGuard loopGuard(loopDepth);

		//Имя создаваемого итератора
		node->iteratorName = expect(TokenType::Identifier).value;

		expect(TokenType::Keyword, "in");

		//Цель обхода
		node->iterable = parseExpression();

		//Тело цикла
		node->body = parseBlock(); //Не допускает отсутствия фигурных скобок

		return node;
	}

	AST::StatementPtr Parser::parseReturnStatement()
	{
		Token token = expect(TokenType::Keyword, "return");

		auto node = std::make_unique<AST::ReturnStatement>();

		//Собираем "кортеж" из возвращаемых значений
		do
		{
			auto stmt = parseExpression();
			node->values.push_back(std::move(stmt));

		} while (match(TokenType::COMMA));

		return node;
	}

	AST::StatementPtr Parser::parseContinueStatement()
	{
		Token token = expect(TokenType::Keyword, "continue");

		if (loopDepth > 0)
			return std::make_unique<AST::ContinueStatement>();

		//Ключевое слово continue вне цикла вызывает ошибку
		throw ParseError(getLocation(token), ReportCode::ContinueOutsideLoop);
	}

	AST::StatementPtr Parser::parseBreakStatement()
	{
		Token token = expect(TokenType::Keyword, "break");

		if (loopDepth > 0)
			return std::make_unique<AST::BreakStatement>();

		//Ключевое слово break вне цикла вызывает ошибку
		throw ParseError(getLocation(token), ReportCode::BreakOutsideLoop);
	}

	AST::StatementPtr Parser::parseWhenStatement()
	{
		/* EXAMPLE:
		   int day = 5
		   int holiday = 3
		   when day //auto breaks after executing the matched case block, no need for explicit break statements
		   {
		       holiday => { print("It's a holiday") }
		       1, 2, 3, 4, 5 => { print("It's a weekday") }
		       6, 7 => { print("It's a weekend") }
		       default => { print("Invalid day") }
		   }
		*/
		Token token = expect(TokenType::Keyword, "when");

		auto node = std::make_unique<AST::WhenStatement>();

		node->value = parseExpression();

		expect(TokenType::LBRACE); //{

		while (!check(TokenType::END) && !check(TokenType::RBRACE))
		{
			Token token = stream.peek();
			if (!match(TokenType::Keyword, "default"))
			{
				node->cases.push_back(parseWhenCase());
				continue;
			}
		
			//default case:

			expect(TokenType::Operator, "=>");

			auto defaultCase = std::make_unique<AST::WhenCase>();
			defaultCase->body = parseBlock();

			node->defaultCase = std::move(defaultCase);

			if (!check(TokenType::RBRACE))
				throw ParseError(getLocation(token), ReportCode::DefaultCaseMustBeLast);

			break;//Гарантирует, что default case объявлен последним	
		}

		expect(TokenType::RBRACE); //}

		return node;
	}
	std::unique_ptr<AST::WhenCase> Parser::parseWhenCase()
	{
		auto node = std::make_unique<AST::WhenCase>();

		//Собираем выражения через запятую
		do
		{
			auto expr = parseExpression();
			node->conditions.push_back(std::move(expr));

		} while (match(TokenType::COMMA));

		expect(TokenType::Operator, "=>");

		node->body = parseBlock();
		node->fallthrough = match(TokenType::Keyword, "cascade");

		return node;
	}

	//Работает с '(', списком аргументов (через запятую) и ')'
	std::vector<AST::ExpressionPtr> Parser::parseArgList()
	{
		std::vector<AST::ExpressionPtr> arguments;

		//Преждевременный выход из функции во избежание попытки парсинга пустого набора аргументов
		if (check(TokenType::RPAREN))//')'
			return arguments;

		//Сбор аргументов через запятую
		do
		{
			arguments.push_back(parseExpression());

		} while (match(TokenType::COMMA)); //','


		return arguments;
	}

	AST::ExpressionPtr Parser::parseContainerLiteralExpression()
	{
		expect(TokenType::LBRACKET);

		auto node = std::make_unique<AST::ContainerLiteralExpression>();

		if (!check(TokenType::RBRACKET))
		{
			do
			{
				auto expr = parseExpression();
				node->values.push_back(std::move(expr));

			} while (match(TokenType::COMMA));
		}

		expect(TokenType::RBRACKET);

		return node;
	}


	std::unique_ptr<AST::NamespaceDeclaration> Parser::parseNamespaceDeclaration()
	{
		expect(TokenType::Keyword, "namespace");

		auto node = std::make_unique<AST::NamespaceDeclaration>();

		//Получение имени пространства имён
		node->name = parseQualifiedName();

		expect(TokenType::LBRACE);

		while (!check(TokenType::END) && !check(TokenType::RBRACE))
		{
			node->members.push_back(parseTopLevelDeclaration());
		}

		expect(TokenType::RBRACE);

		return node;
	}

	//TODO
	std::unique_ptr<AST::RuntimeHookDeclaration> Parser::parseRuntimeHookDeclaration()
	{
		expect(TokenType::Keyword, "at");

		auto node = std::make_unique<AST::RuntimeHookDeclaration>();

		node->hookName = parseQualifiedName();

		node->body = parseBlock();

		return node;
	}

	std::unique_ptr<AST::ImportDeclaration> Parser::parseImportDeclaration()
	{
		expect(TokenType::Keyword, "import");

		auto node = std::make_unique<AST::ImportDeclaration>();

		//Разбор одиночного импорта (без фигурных скобок)
		if (!match(TokenType::LBRACE))
		{
			node->imported.push_back(parseImportItem());
			return node;
		}

		//Преждевременное завершение блока импорта (import {})
		
		Token t = stream.peek();
		if (match(TokenType::RBRACE))
		{
			warning(t, ReportCode::EmptyImportList, "Import block is empty.");
			return node;
		}
		
		//Разбор блока иморта (импорты через запятую)
		while (true)
		{
			node->imported.push_back(parseImportItem());

			if (!match(TokenType::COMMA))
				break;

			if (check(TokenType::RBRACE))
				break;
		}

		expect(TokenType::RBRACE);
		return node;
	}
	AST::ImportItem Parser::parseImportItem()
	{
		AST::ImportItem item;

		item.sourcePath = expect(TokenType::String).value;

		//Псевдоним для namespace-оболочки
		if (!match(TokenType::Keyword, "as"))
			return item;

		item.alias = expect(TokenType::Identifier).value;
		return item;
	}

	std::unique_ptr<AST::InterfaceDeclaration> Parser::parseInterfaceDeclaration()
	{
		expect(TokenType::Keyword, "interface");

		auto node = std::make_unique<AST::InterfaceDeclaration>();

		Token t = stream.peek(); //токен для диагностики

		//Имя интерфейса
		node->name = expect(TokenType::Identifier).value;
		expect(TokenType::LBRACE);

		
		if (match(TokenType::RBRACE))
		{
			warning(t, ReportCode::EmptyInterface, "Interface declares no methods.");
			return node;
		}

		while (!check(TokenType::END) && !check(TokenType::RBRACE))
		{
			node->methods.push_back(parseInterfaceItem());
		}

		expect(TokenType::RBRACE);
		return node;
	}

	AST::InterfaceItem Parser::parseInterfaceItem()
	{
		AST::InterfaceItem item;
		Token tname = expect(TokenType::Identifier);

		//Имя функции
		item.name = tname.value;
		item.params = parseParameterList();


		if(!match(TokenType::Keyword, "void"))
			item.returns = parseReturnTypeList();

		return item;
	}

	std::unique_ptr<AST::FuncDeclaration> Parser::parseFuncDeclaration()
	{
		expect(TokenType::Keyword, "def");

		auto node = std::make_unique<AST::FuncDeclaration>();
		node->name = expect(TokenType::Identifier).value;

		node->params = parseParameterList();

		if (!check(TokenType::LBRACE))
			node->returns = parseReturnTypeList();

		node->body = parseBlock();

		return node;
	}


	std::unique_ptr<AST::ConstructorDeclaration> Parser::parseConstructorDeclaration()
	{
		/*
			constructor(params) {}
			constructor() {}
			constructor {]
		*/
		auto node = std::make_unique<AST::ConstructorDeclaration>();
		expect(TokenType::Keyword, "constructor");

		//Поддержка опциональности круглых скобок для параметров
		if (check(TokenType::LPAREN))
			node->params = parseParameterList();

		node->body = parseBlock();

		return node;
	}
	std::unique_ptr<AST::DestructorDeclaration>  Parser::parseDestructorDeclaration()
	{
		/*
			destructor {}
		*/
		auto node = std::make_unique<AST::DestructorDeclaration>();
		expect(TokenType::Keyword, "destructor");

		//Деструктор не принимает параметров, круглые скобки не ставятся
		node->body = parseBlock();
		return node;
	}
	std::unique_ptr<AST::ClassDeclaration>  Parser::parseClassDeclaration()
	{
		auto node = std::make_unique<AST::ClassDeclaration>();

		expect(TokenType::Keyword, "class");

		//Имя класса
		node->name = expect(TokenType::Identifier).value;

		//Опциональный список шаблонных типов
		if (match(TokenType::LPAREN))
		{
			do
			{
				node->typeParams.push_back(parseGenericArgument());
			} while (match(TokenType::COMMA));

			expect(TokenType::RPAREN);
		}

		//Опциональный список интерфейсов
		if (match(TokenType::Keyword, "with"))
		{
			do
			{
				node->interfaces.push_back(parseQualifiedName());
			} while (match(TokenType::COMMA));
		}



		expect(TokenType::LBRACE);


		AccessModifier access = DefaultAccessModifier;

		auto collectMembersInSection = [&]()
			{
				while (!check(TokenType::END) && !check(TokenType::RBRACE))
				{
					//Конструктор (деструктор объявляется вне секций инкапсуляции)
					if (check(TokenType::Keyword, "constructor"))
					{
						AST::ClassDeclaration::Member constructor;
						constructor.member = parseConstructorDeclaration();
						constructor.access = access; //установка уровня инкапсуляции

						node->members.push_back(std::move(constructor));
						continue;
					}
					//Обычный член класса (поле, метод)
					else node->members.push_back(parseClassMember(access));
				}
			};


		while (!check(TokenType::END) && !check(TokenType::RBRACE))
		{
			access = DefaultAccessModifier;

			Token t = stream.peek();
			if (check(TokenType::Keyword, "constructor"))
			{
				AST::ClassDeclaration::Member constructor;
				constructor.member = parseConstructorDeclaration();
				constructor.access = access;

				node->members.push_back(std::move(constructor));
				continue;
			}
			else if (check(TokenType::Keyword, "destructor"))
			{
				if (node->destructor)
					throw ParseError(getLocation(t), ReportCode::RedefiningDestructor, "Destructor already defined.");

				node->destructor = parseDestructorDeclaration();
				continue;
			}
			if (match(TokenType::Keyword, "public"))
				access = AccessModifier::Public;
			else if (match(TokenType::Keyword, "private"))
				access = AccessModifier::Private;
			else if (match(TokenType::Keyword, "protected"))
				access = AccessModifier::Protected;
			else
			{
				//Разбор объявления вне секций инкапсуляции
				node->members.push_back(parseClassMember(access));
				continue;
			}

			//Разбор секций private, public, protected
			expect(TokenType::LBRACE);
			collectMembersInSection(); 
			expect(TokenType::RBRACE);
		}

		expect(TokenType::RBRACE);

		return node;
	}
	AST::ClassDeclaration::Member Parser::parseClassMember(AccessModifier modifier)
	{
		AST::ClassDeclaration::Member member;

		member.access = modifier;

		if (check(TokenType::Keyword, "def"))
			member.member = parseFuncDeclaration(); //метод
		else
			member.member = parseVariableDeclarationStatement(); //поле

		return member;
	}

	AST::DeclarationPtr Parser::parseVariableDeclarationStatement()
	{
		auto node = std::make_unique<AST::DeclarationStatement>();

		node->objectType = parseTypeRef(); //модификаторы (const, ref) и тип объекта
		node->objectName = expect(TokenType::Identifier).value; //имя объекта

		//Опциональный инициализатор
		if (match(TokenType::Operator, "="))
			node->initializer = parseExpression(); 

		return node;
	}

	//Инструкция-обёртка над выражением
	AST::StatementPtr Parser::parseExpressionStatement()
	{
		auto node = std::make_unique<AST::ExpressionStatement>();
		node->expression = parseExpression();

		return node;
	}

	AST::StatementPtr Parser::parseStatement()
	{
		Token token = stream.peek();

		if (token.type == TokenType::Keyword)
		{
			if (token.value == "if")
				return parseIfStatement();

			if (token.value == "while")
				return parseWhileStatement();

			if (token.value == "for")
				return parseForStatement();

			if (token.value == "when")
				return parseWhenStatement();

			if (token.value == "return")
				return parseReturnStatement();

			if (token.value == "break")
				return parseBreakStatement();

			if (token.value == "continue")
				return parseContinueStatement();
		}


		if (token.type == TokenType::Identifier)
		{
			if (isAmbiguousConstruct())
				return parseAmbiguousStatement();
		}

		return parseExpressionStatement();
	}

	AST::DeclarationPtr Parser::parseTopLevelDeclaration()
	{
		Token token = stream.peek();
	
		if (token.type == TokenType::Keyword)
		{
			if (token.value == "import")
				return parseImportDeclaration();
	
			if (token.value == "namespace")
				return parseNamespaceDeclaration();
	
			if (token.value == "class")
				return parseClassDeclaration();
	
			if (token.value == "interface")
				return parseInterfaceDeclaration();
	
			if (token.value == "def")
				return parseFuncDeclaration();
	
			/*
				TODO:
				Lifecycle hooks:
				at init
				at start
				at exit
				at panic
				at custom hook
			*/
		}
	
		throw ParseError(getLocation(stream.peek()), ReportCode::UnexpectedToken,"Expected top-level declaration");
	}

	AST::StatementPtr Parser::parseTopLevelItem()
	{
		if (isTopLevelDeclaration())
			return parseTopLevelDeclarationSafely();

		return parseStatementSafely();
	}

	bool Parser::isTopLevelDeclaration() const
	{
		switch (stream.peek().type)
		{
		case TokenType::Keyword:
		{
			const auto& k = stream.peek().value;

			return k == "import" ||
				k == "namespace" ||
				k == "class" ||
				k == "interface" ||
				k == "def";
		}

		default:
			return false;
		}
	}

	AST::TranslationUnit Parser::parse()
	{
		AST::TranslationUnit unit;

		while (!check(TokenType::END))
		{
			unit.statements.push_back(parseTopLevelItem());
		}

		return unit;
	}
}