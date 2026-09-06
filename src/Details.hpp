#pragma once
#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <memory>

namespace Lauter
{


	enum class LiteralType : uint8_t
	{
		Int,
		UInt,
		Real,
		Boolean,
		String
	};

	using LiteralValue = std::variant<
		int64_t,
		uint64_t,
		double,
		bool,
		std::string
	>;

	enum class AccessModifier : uint8_t
	{
		Public,
		Private,
		Protected
	};
	constexpr AccessModifier DefaultAccessModifier = AccessModifier::Public;

	//Бинарные операторы
	enum class BinaryOperator : uint8_t
	{
		// Arithmetic
		Add,
		Sub,
		Mul,
		Div,
		Mod,
		Power,

		// Comparison
		Equal,
		NotEqual,
		StrictEqual,

		Less,
		Greater,
		LessEqual,
		GreaterEqual,

		// Logical
		LogicalAnd,
		LogicalOr,

		// Bitwise
		BitwiseAnd,
		BitwiseOr,
		BitwiseXor,
		ShiftLeft,
		ShiftRight
	};

	//Операторы присвоения
	enum class AssignmentOperator
	{
		Assign,
		AddAssign,
		SubAssign,
		MulAssign,
		DivAssign,
		ModAssign,
		BitAndAssign,
		BitOrAssign,
		BitXorAssign,
		ShiftLeftAssign,
		ShiftRightAssign
	};


	//Унарные операторы
	enum class UnaryOperator : uint8_t
	{
		Negate,
		Positive,
		LogicalNot,
		BitwiseNot
	};

	//Позиция в коде
	struct SourceLocation
	{
		size_t row = 0;
		size_t column = 0;
	};

	/*
		SemanticType — семантический тип, хранящийся в таблице символов (принадлежит таблице символов)
		TypeRef — сементический тип с квалификаторами, представленный в исходном коде программы (принадлежит синтаксическому анализатору)
		QualifiedType — разрешённый TypeRef с ссылкой на семантический тип из таблицы символов (принадлежит семантическому анализатору и сборщику кода)
	*/

	struct SemanticType;
	
	//Составное имя с указанием пространств имён (оператор ::)
	struct QualifiedName
	{
		std::vector<std::string> parts;

		const std::string& shortName() const
		{
			assert(!parts.empty());
			return parts.back();
		}

		//Соединяет части полного имени через '::'
		std::string toString() const
		{
			if (parts.empty())
				return "";

			size_t size = 0;

			for (const auto& part : parts)
				size += part.size() + 2;

			std::string res;
			res.reserve(size);

			res += parts[0];

			for (size_t i = 1; i < parts.size(); i++)
			{
				res += "::";
				res += parts[i];
			}

			return res;
		}
	};



	struct TypeRef;
	
	//Шаблонный тип или литерал
	struct GenericArgument
	{
		SourceLocation location;

		//TODO: constexpr expressions
		std::variant<
			std::unique_ptr<TypeRef>,
			LiteralValue
		> value;
	};


	//Тип + модификаторы
	struct QualifiedType
	{
		//Тип, представленный в таблице символов
		const SemanticType* type = nullptr;

		//Является ли константой
		bool isConst = false;

		//Является ли ссылкой
		bool isReference = false;

		std::vector<QualifiedType> generics;

		bool operator==(const QualifiedType& other) const
		{
			return type == other.type &&
				   isConst == other.isConst &&
				   isReference == other.isReference &&
				   generics == other.generics;
		}
	};

	struct TypeRef
	{
		SourceLocation location;
		QualifiedName typeName;

		//Является ли константой
		bool isConst = false;

		//Является ли ссылкой
		bool isReference = false;

		std::vector<GenericArgument> generics;
	};


	enum class TypeKind
	{
		Primitive, //int, real, bool
		GenericParameter,
		Class, //пользовательские типы
		Function, //для ссылок на функции
		Interface, //для ссылки на сущность произвольного класса, реализующего данный интерфейс
		Enum, //для перечислений (на будущее)
		Alias, //псевдоним
		Unknown
	};

	enum class SymbolKind
	{
		Type,              // имя типа в области видимости (примитив, пользовательский тип, интерфейс)
		FunctionOverloads,          // объявленная функция с перегрузками
		Variable,          // переменная
		Parameter,         // параметр функции
		Namespace,         // пространство имён
		GenericParameter   // T в class Box(T)
	};



	//Сигнатура функции/метода (семантический анализ)
	struct FunctionSignature
	{
		std::vector<QualifiedType> parameters;
		std::vector<QualifiedType> returns;

	};

	struct FunctionType;

	//Тип для таблицы символов
	struct SemanticType
	{
		QualifiedName fullname;
		TypeKind kind;

		SemanticType(QualifiedName fullname, TypeKind kind)
			: fullname(std::move(fullname)), kind(kind)
		{
		}
		virtual ~SemanticType() = default;
	};

	//Псевдоним
	struct AliasType : SemanticType
	{
		SemanticType* target;

		AliasType(
			QualifiedName name,
			SemanticType* target
		)
			:
			SemanticType(
				std::move(name),
				TypeKind::Alias
			),
			target(target)
		{
		}
	};

	//Раскрытие псевдонима
	inline const SemanticType* resolveAlias(const SemanticType* type)
	{
		std::unordered_set<const SemanticType*> visited;
		while (type && type->kind == TypeKind::Alias)
		{
			if (!visited.insert(type).second)
				return nullptr; //Цикл псевдонимов

			type = static_cast<const AliasType*>(type)->target;
		}
		return type;
	}


	//Тип для таблицы символов, который хранится в памяти
	struct DataType : SemanticType
	{
		size_t size = 0;
		size_t alignment = 0;

		DataType(QualifiedName fullname, TypeKind kind, size_t size, size_t alignment)
			: SemanticType(std::move(fullname), kind), size(size), alignment(alignment)
		{}
	};

	//int, real, string, bool
	struct PrimitiveType : DataType
	{
		PrimitiveType(QualifiedName name, size_t size, size_t alignment)
			: DataType(std::move(name), TypeKind::Primitive, size, alignment )
		{ }
	};
	
	struct GenericParameterType : SemanticType
	{
		size_t index = 0;

		GenericParameterType(QualifiedName name, size_t index)
			: SemanticType(std::move(name), TypeKind::GenericParameter),
			index(index)
		{
		}
	};

	//Семантический тип для отличия условного List(T) от List(E)
	struct GenericInstanceType : SemanticType
	{
		SemanticType* base;
		std::vector<QualifiedType> arguments;
	};

	struct InterfaceMethod
	{
		std::string name;
		FunctionType* type;
	};

	//Тип для создания ссылки на объект, реализующий данный интерфейс
	struct InterfaceType : SemanticType
	{
		InterfaceType(QualifiedName name)
			: SemanticType(std::move(name), TypeKind::Interface)
		{}
		std::vector<InterfaceMethod> methods;
	};

	//Тип для создания ссылки на функцию
	struct FunctionType : SemanticType
	{
		FunctionSignature signature;

		FunctionType(QualifiedName name, FunctionSignature signature)
			: SemanticType(std::move(name), TypeKind::Function), signature(std::move(signature))
		{}
	};

	//Поле класса
	struct Field
	{
		std::string name;

		QualifiedType type;
		AccessModifier access = DefaultAccessModifier;

		size_t offset = 0;

		bool isStatic = false; //Модификатор для объявления полей, не привязанных к конкретному объекту
	};


	struct ClassMethod
	{
		std::string name;

		FunctionType* type;
		AccessModifier access = DefaultAccessModifier;

		bool isStatic = false; //Модификатор для объявления методов, не привязанных к конкретному объекту
	};


	struct ClassType : DataType
	{
		std::unordered_set<const InterfaceType*> interfaces;

		std::unordered_map<std::string, Field> fields;
		std::unordered_map<std::string, ClassMethod> methods;

		ClassType(QualifiedName name, size_t size, size_t alignment)
			: DataType(std::move(name), TypeKind::Class, size, alignment)
		{}
	};

	struct EnumValue
	{
		std::string name;
		int64_t value = 0;
	};

	struct EnumType : DataType
	{
		std::vector<EnumValue> values;
	};

	

	inline const char* binaryOperatorFunctionName(BinaryOperator op)
	{
		switch (op)
		{
		case BinaryOperator::Add:       return "operator_add";
		case BinaryOperator::Sub:       return "operator_sub";
		case BinaryOperator::Mul:       return "operator_mul";
		case BinaryOperator::Div:       return "operator_div";
		case BinaryOperator::Mod:       return "operator_mod";
		case BinaryOperator::Power:     return "operator_pow*";

		case BinaryOperator::Equal:        return "operator_equal";
		case BinaryOperator::NotEqual:     return "operator_not_equal";
		case BinaryOperator::StrictEqual:  return "operator_strict_equal";

		case BinaryOperator::Less:         return "operator_less";
		case BinaryOperator::Greater:      return "operator_greater";
		case BinaryOperator::LessEqual:    return "operator_less_equal";
		case BinaryOperator::GreaterEqual: return "operator_greater_equal";

		case BinaryOperator::LogicalAnd:   return "operator_and";
		case BinaryOperator::LogicalOr:    return "operator_or";

		case BinaryOperator::BitwiseAnd:   return "operator_bitwise_and";
		case BinaryOperator::BitwiseOr:    return "operator_bitwise_or";
		case BinaryOperator::BitwiseXor:   return "operator_bitwise_xor";
		case BinaryOperator::ShiftLeft:    return "operator_left_shift";
		case BinaryOperator::ShiftRight:   return "operator_right_shift";
		}
		assert(false && "Unknown BinaryOperator");
		return "";
	}

}

