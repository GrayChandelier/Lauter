#pragma once
#include "AST.hpp"
#include "Lexer.hpp"
#include "Diagnostic.hpp"

namespace Lauter
{
	class ITokenStream
	{
	public:
		virtual bool end() const = 0;
		virtual const Token& peek(size_t offset = 0) const = 0;
		virtual Token get() = 0;
		virtual ~ITokenStream() = default;
	};
	class TokenStreamReader : public ITokenStream
	{
	private:
		const std::vector<Token>& tokenStream;
		size_t current = 0;
	public:
		TokenStreamReader(const std::vector<Token>& tokens)
			: tokenStream(tokens)
		{
		}
		bool end() const noexcept override;
		const Token& peek(size_t offset = 0) const noexcept override;
		Token get() noexcept override;
	};

	//Ћокальное исключение control-flow парсера. Ќе пересекает границу Parser Ч
	//ловитс€ в safe-обЄртках, которые вызывают synchronize().
	struct ParseError : std::exception
	{
		SourceLocation location;
		std::string message;
		ReportCode code = ReportCode::UNKNOWN;

		ParseError(SourceLocation location, std::string message)
			: location(location), message(std::move(message)) { }

		ParseError(SourceLocation location, ReportCode code)
			: location(location), code(code) {
		}

		ParseError(SourceLocation location, ReportCode code, std::string message)
			: location(location), code(code), message(std::move(message)) {
		}

		const char* what() const noexcept override { return message.c_str(); }
	};

	class Parser
	{
	private:
		ITokenStream& stream;
		DiagnosticEngine& diagnostics;

		//√лубина вложенных циклов дл€ предотвращени€ вызова break или continue вне while или for
		int loopDepth = 0;

		//RAII обЄртка дл€ оператора инкремента/декремента глубины цикла
		struct LoopGuard
		{
			int& _loopDepth;

			LoopGuard() = delete;

			LoopGuard(int& loopDepth)
				: _loopDepth(loopDepth)
			{
				loopDepth++;
			}

			~LoopGuard()
			{
				_loopDepth--;
			}
		};

		//=== Diagnostics / error recovery ===

		[[noreturn]] void unexpectedToken(const Token& token) const;
		[[noreturn]] void unexpectedToken(const Token& token, TokenType expectedType) const;

		//helper
		ReportLocation getReportLocation(const Token& token) const;


		//ѕрокручивает поток до ближайшей точки синхронизации (';', '}', ключевое
		//слово начала statement/declaration), не выход€ за границу текущего блока
		void synchronize();
		void synchronizeTopLevel();

		//ћетод, провер€ющий начало инструкции. Ќеобходим дл€ работы метода synchronize()
		bool isStatementStart() const;
		//ћетод, провер€ющий начало объ€влени€. Ќеобходим дл€ работы метода synchronizeTopLevel()
		bool isTopLevelStart() const;

		//‘абрики error-узлов Ч обща€ точка, куда попадает SourceLocation последней
		//валидной позиции перед сбоем
		AST::ExpressionPtr makeErrorExpression(SourceLocation location) const;
		AST::StatementPtr makeErrorStatement(SourceLocation location) const;
		AST::DeclarationPtr makeErrorDeclaration(SourceLocation location) const;

		//Safe-обЄртки: лов€т ParseError, вызывают synchronize(), возвращают
		//соответствующую Error-ноду вместо nullptr/исключени€ наружу
		AST::StatementPtr parseStatementSafely();
		AST::DeclarationPtr parseTopLevelDeclarationSafely();

		//=== Token matching ===

		bool check(TokenType type, const std::string& value = "") const;
		bool match(TokenType type, const std::string& value = "");
		Token expect(TokenType type, const std::string& value = "");

	
		inline void warning(const Token& token, ReportCode code, const std::string& message)
		{
			Report report;
			report.location = getReportLocation(token);
			report.message = message;
			report.severity = Severity::Warning;
			report.code = code;

			diagnostics.report(std::move(report));
		}
		//=== Types / qualified names ===

		QualifiedName parseQualifiedName();
		TypeRef parseTypeRef();
		GenericArgument parseGenericArgument();
		std::vector<GenericArgument> parseGenericArgumentList();
		AST::Parameter parseParameter();
		std::vector<AST::Parameter> parseParameterList();
		std::vector<TypeRef> parseReturnTypeList();

		//=== Expressions (по убыванию приоритета св€зывани€ снизу вверх) ===

		AST::ExpressionPtr parseExpression();
		AST::ExpressionPtr parseAssignment();
		AST::ExpressionPtr parseOr();
		AST::ExpressionPtr parseAnd();
		AST::ExpressionPtr parseComparison();
		AST::ExpressionPtr parseAddSub();
		AST::ExpressionPtr parseMulDiv();
		AST::ExpressionPtr parsePower();
		AST::ExpressionPtr parseUnary();
		AST::ExpressionPtr parsePostfix();
		AST::ExpressionPtr parsePrimary();

		std::vector<AST::ExpressionPtr> parseArgList();
		AST::ExpressionPtr parseContainerLiteralExpression();

		//=== Ambiguity resolution ===

		//true, если впереди последовательность, которую нельз€ разобрать как
		//чистое выражение/чистое объ€вление без символьной таблицы Ч
		//откладываетс€ в AmbiguousStatement
		bool isAmbiguousConstruct() const;
		//–азбор неоднозначных конструкций вида A(x) B(y), которые нельз€ разобрать без знани€ таблицы символов
		AST::StatementPtr parseAmbiguousStatement();

		//=== Statements ===

		
		AST::StatementPtr parseBlock(); //считывает фигурные скобки и список инструкций внутри них
		AST::Body parseBody(); //один statement или Block, в зависимости от синтаксиса
		AST::StatementPtr parseStatement();

		AST::StatementPtr parseExpressionStatement();
		AST::DeclarationPtr parseVariableDeclarationStatement();
		AST::StatementPtr parseIfStatement();
		AST::StatementPtr parseWhileStatement();
		AST::StatementPtr parseForStatement();

		AST::StatementPtr parseWhenStatement();
		std::unique_ptr<AST::WhenCase> parseWhenCase();
		AST::StatementPtr parseReturnStatement();
		AST::StatementPtr parseContinueStatement();
		AST::StatementPtr parseBreakStatement();

		//=== Top-level declarations ===

		//ќбъ€вление сущностей высшего пор€дка (классы, интерфейсы, пространства имЄн и т.п.)
		bool isTopLevelDeclaration() const;
		AST::DeclarationPtr parseTopLevelDeclaration();
		AST::StatementPtr parseTopLevelItem(); //StatementPtr или DeclarationPtr

		std::unique_ptr<AST::FuncDeclaration> parseFuncDeclaration();
		std::unique_ptr<AST::ConstructorDeclaration> parseConstructorDeclaration();
		std::unique_ptr<AST::DestructorDeclaration> parseDestructorDeclaration();
		std::unique_ptr<AST::ClassDeclaration> parseClassDeclaration();
		AST::ClassDeclaration::Member parseClassMember(AccessModifier modifier);
		std::unique_ptr<AST::InterfaceDeclaration> parseInterfaceDeclaration();
		AST::InterfaceItem parseInterfaceItem();
		std::unique_ptr<AST::ImportDeclaration> parseImportDeclaration();
		AST::ImportItem parseImportItem();
		std::unique_ptr<AST::NamespaceDeclaration> parseNamespaceDeclaration();

		//W.I.P
		std::unique_ptr<AST::RuntimeHookDeclaration> parseRuntimeHookDeclaration();

	public:
		Parser(ITokenStream& tokenStream, DiagnosticEngine& diagnostic)
			: stream(tokenStream), diagnostics(diagnostic){
		}
		Parser(const Parser&) = delete;
		Parser& operator=(const Parser&) = delete;

		Parser(Parser&&) = delete;
		Parser& operator=(Parser&&) = delete;


		AST::TranslationUnit parse();
	};
} 