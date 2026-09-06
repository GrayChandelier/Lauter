#pragma once
#include <cinttypes>
#include <string>
#include <variant>
#include <memory>
#include <vector>

#include "Details.hpp"

//Листовые узлы, представляющие выражение
#define LAUTER_AST_EXPR_NODES(X) \
    X(Literal) \
    X(BinaryExpression) \
    X(UnaryExpression) \
    X(PostfixExpression) \
    X(IdentifierExpression) \
    X(ContainerLiteralExpression) \
    X(AssignmentExpression) \
	X(ConversionExpression) \
	X(InterfaceCastExpression) \
    X(ErrorExpression)

//Листовые узлы, представляющие инструкцию
#define LAUTER_AST_STMT_NODES(X) \
    X(Block) \
    X(AmbiguousChain) \
    X(ExpressionStatement) \
    X(IfStatement) \
    X(WhileStatement) \
    X(ForInStatement) \
    X(ContinueStatement) \
    X(BreakStatement) \
    X(ReturnStatement) \
    X(WhenStatement) \
    X(DeclarationStatement) \
    X(NamespaceDeclaration) \
    X(FuncDeclaration) \
    X(ConstructorDeclaration) \
    X(DestructorDeclaration) \
    X(ClassDeclaration) \
    X(InterfaceDeclaration) \
    X(ImportDeclaration) \
    X(RuntimeHookDeclaration) \
    X(ErrorStatement) \
    X(ErrorDeclaration)



//Макросы для Expression::accept и Statement::accept
#define LAUTER_ACCEPT_EXPR_DECL QualifiedType accept(ExpressionVisitor& visitor) override;
#define LAUTER_ACCEPT_STMT_DECL void accept(StatementVisitor& visitor) override;

//Вспомогательные структуры
namespace Lauter::AST
{

	//Макрос для листовых узлов AST
#define X(name) class name;
	LAUTER_AST_EXPR_NODES(X)
	LAUTER_AST_STMT_NODES(X)
#undef X

	class Expression;
	using ExpressionPtr = std::unique_ptr<Expression>;

	struct IndexOperation
	{
		ExpressionPtr index;
	};
	struct MemberAccessOperation
	{
		std::string member;
	};
	struct CallOperation
	{
		std::vector<ExpressionPtr> arguments;
	};
	using PostfixOperation = std::variant<IndexOperation, MemberAccessOperation, CallOperation>;
}

namespace Lauter::AST
{
	class ExpressionVisitor;
	class StatementVisitor;

	//Абстрактный узел синтаксического дерева
	class ASTNode
	{
	protected:
		SourceLocation location;

	public:
		explicit ASTNode(SourceLocation location = {})
			: location(location)
		{
		}

		SourceLocation getLocation() const
		{
			return location;
		}

		virtual ~ASTNode() = default;
	};

	//Выражение
	class Expression : public ASTNode
	{
	protected:
		using ASTNode::ASTNode;
	public:
		virtual QualifiedType accept(ExpressionVisitor& visitor) = 0;
	};

	//Литералы
	class Literal : public Expression
	{
	public:
		//значение литерала
		LiteralValue value;

		//kind необходим для представления типов, которые не зависят от C++ (int8, int16, int32, int64 и т.п.)
		LiteralType kind;

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Бинарное выражение
	class BinaryExpression : public Expression
	{
	public:
		BinaryOperator op;
		ExpressionPtr leftOperand;
		ExpressionPtr rightOperand;

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Унарное выражение
	class UnaryExpression : public Expression
	{
	public:
		UnaryOperator op;
		ExpressionPtr operand;

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Постфиксные выражения (доступ к полям, оператор индекса, вызов функции/метода)
	class PostfixExpression : public Expression
	{
	public:
		ExpressionPtr target;
		std::vector<PostfixOperation> operations;

		PostfixExpression() = default;

		PostfixExpression(SourceLocation location, ExpressionPtr target)
			: Expression(location), target(std::move(target)) {
		}

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Объект, константа или переменная
	class IdentifierExpression : public Expression
	{
	public:
		QualifiedName name;

		IdentifierExpression(SourceLocation location, QualifiedName name)
			: Expression(location), name(std::move(name))
		{
		}

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Кортеж
	class ContainerLiteralExpression : public Expression
	{
	public:
		std::vector<ExpressionPtr> values;

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Присвоение значения
	class AssignmentExpression : public Expression
	{
	public:
		AssignmentOperator op;
		ExpressionPtr target;
		ExpressionPtr value;

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Инструкции
	class Statement : public ASTNode
	{
	protected:
		using ASTNode::ASTNode;
	public:
		virtual void accept(StatementVisitor& visitor) = 0;
	};
	using StatementPtr = std::unique_ptr<Statement>;

	//Объявление
	class Declaration : public Statement
	{
	protected:
		using Statement::Statement;
	};
	using DeclarationPtr = std::unique_ptr<Declaration>;

	//Тело условной конструкции, цикла или функции {}
	class Block : public Statement
	{
	public:
		std::vector<StatementPtr> statements;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Тело условной конструкции или цикла
	using Body = StatementPtr;

	//Структура для разрешения неоднозначных конструкций с помощью таблицы символов
	/* Примеры неоднозначных конструкций:
	*     array(int, 3) myArray = [1,2,3] — объявление объекта myArray шаблонного типа array с присвоением заданных значений
	*     func(10) myVar = 5 — вызов функции func и присвоение значения 5 переменной myVar
	*/
	class AmbiguousChain : public Statement
	{
	public:
		struct Segment
		{
			SourceLocation location;

			QualifiedName ident;

			std::vector<ExpressionPtr> arguments;

			Segment(SourceLocation location, QualifiedName name)
				: location(location), ident(name) {
			}
		};

		std::vector<Segment> segments;

		ExpressionPtr lastSegmentInitializer = nullptr;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Инструкция-выражение
	class ExpressionStatement : public Statement
	{
	public:
		ExpressionPtr expression;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Условная конструкция
	class IfStatement : public Statement
	{
	public:
		ExpressionPtr condition;
		Body thenBody;
		Body elseBody = nullptr;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Цикл While
	class WhileStatement : public Statement
	{
	public:
		ExpressionPtr condition;
		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Цикл for-in
	class ForInStatement : public Statement
	{
	public:
		std::string iteratorName;
		ExpressionPtr iterable;

		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Пропуск шага цикла через continue
	class ContinueStatement : public Statement
	{
	public:
		LAUTER_ACCEPT_STMT_DECL;
	};
	//Завершение цикла через break
	class BreakStatement : public Statement
	{
	public:
		LAUTER_ACCEPT_STMT_DECL;
	};

	//Возвращение значений из функции/метода
	class ReturnStatement : public Statement
	{
	public:
		std::vector<ExpressionPtr> values;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Дочерние узлы конструкции When
	class WhenCase : public ASTNode
	{
	public:
		std::vector<ExpressionPtr> conditions;
		Body body;

		//Пропуск автоматического break
		bool fallthrough = false;
	};
	using WhenCasePtr = std::unique_ptr<WhenCase>;

	//Условная конструкция When
	class WhenStatement : public Statement
	{
	public:
		ExpressionPtr value;

		std::vector<WhenCasePtr> cases;
		WhenCasePtr defaultCase = nullptr;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Объявление объекта, константы или переменной
	class DeclarationStatement : public Declaration
	{
	public:
		TypeRef objectType;
		std::string objectName;
		ExpressionPtr initializer = nullptr;

		LAUTER_ACCEPT_STMT_DECL;
	};


	class Parameter : public ASTNode
	{
	public:
		TypeRef type;
		std::string name;

		ExpressionPtr defaultValue = nullptr;
		Parameter(SourceLocation location)
			: ASTNode(location) {
		}
	};

	//Объявление пространства имён
	class NamespaceDeclaration : public Declaration
	{
	public:
		QualifiedName name;

		std::vector<DeclarationPtr> members;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Объявление функции
	class FuncDeclaration : public Declaration
	{
	public:
		//Имя функции
		std::string name;

		//Список параметров
		std::vector<Parameter> params;

		//Список возвращаемых типов
		std::vector<TypeRef> returns;

		//Тело функции
		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Конструктор класса
	class ConstructorDeclaration : public Declaration
	{
	public:
		//Список параметров
		std::vector<Parameter> params;

		//Тело конструктора
		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Деструктор класса
	class DestructorDeclaration : public Declaration
	{
	public:
		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	class ClassDeclaration : public Declaration
	{
	public:
		//Поле класса
		struct Member
		{
			DeclarationPtr member;

			//Уровень доступа
			AccessModifier access;
		};

		//Имя класса
		std::string name;

		//Необходимые интерфейсы
		std::vector<QualifiedName> interfaces;

		//Шаблонные параметры
		std::vector<GenericArgument> typeParams;

		//Поля класса
		std::vector<Member> members;

		//Деструктор
		std::unique_ptr<DestructorDeclaration> destructor;

		LAUTER_ACCEPT_STMT_DECL;
	};

	class InterfaceItem : public ASTNode
	{
	public:
		//Имя метода
		std::string name;

		//Список параметров
		std::vector<Parameter> params;

		//Список возвращаемых типов
		std::vector<TypeRef> returns;
	};

	class InterfaceDeclaration : public Declaration
	{
	public:
		//Имя интерфейса
		std::string name;

		//Методы интерфейса
		std::vector<InterfaceItem> methods;

		LAUTER_ACCEPT_STMT_DECL;
	};

	class ImportItem : public ASTNode
	{
	public:
		std::string sourcePath;

		//Псевдоним для namespace-оболочки, в которую импортируются символы (если пустой, импорт происходит в global namespace)
		std::string alias;


		inline bool hasAlias() const noexcept
		{
			return !alias.empty();
		}
	};

	class ImportDeclaration : public Declaration
	{
	public:
		std::vector<ImportItem> imported;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Lifecycle-блоки (at init, at panic, at start, at exit)
	class RuntimeHookDeclaration : public Declaration
	{
	public:
		QualifiedName hookName; //'exit', 'init', 'panic', 'start' or custom hook
		Body body;

		LAUTER_ACCEPT_STMT_DECL;
	};

	//Корень AST
	class TranslationUnit : public ASTNode
	{
	public:
		std::vector<StatementPtr> statements;
	};


	//Заглушки для движка диагностики:

	class ErrorExpression : public Expression
	{
	public:
		using Expression::Expression;

		LAUTER_ACCEPT_EXPR_DECL;
	};
	class ErrorStatement : public Statement
	{
	public:
		using Statement::Statement;

		LAUTER_ACCEPT_STMT_DECL;
	};
	class ErrorDeclaration : public Declaration
	{
	public:
		using Declaration::Declaration;

		LAUTER_ACCEPT_STMT_DECL;
	};


	//Специальный узел преобразования типов, вставляемый в AST семантическим анализатором
	class ConversionExpression : public Expression
	{
	private:
		ExpressionPtr operand;
		QualifiedType targetType;

	public:
		ConversionExpression(
			ExpressionPtr operand,
			QualifiedType targetType)
			: operand(std::move(operand)),
			targetType(std::move(targetType))
		{
		}

		const Expression* getOperand() const
		{
			return operand.get();
		}

		const QualifiedType& getTargetType() const
		{
			return targetType;
		}

		LAUTER_ACCEPT_EXPR_DECL;
	};

	//Специальный узел преобразования типов, вставляемый в AST семантическим анализатором
	//Приведение объекта к типу реализуемого им интерфейса (upcast, без изменения представления)
	class InterfaceCastExpression : public Expression
	{
	private:
		ExpressionPtr operand;
		QualifiedType targetInterface;
	public:
		InterfaceCastExpression(ExpressionPtr operand, QualifiedType targetInterface)
			: operand(std::move(operand)), targetInterface(std::move(targetInterface))
		{
		}

		Expression* getOperand() { return operand.get(); }
		const QualifiedType& getTargetInterface() const { return targetInterface; }

		LAUTER_ACCEPT_EXPR_DECL;
	};
}

#undef LAUTER_ACCEPT_EXPR_DECL
#undef LAUTER_ACCEPT_STMT_DECL