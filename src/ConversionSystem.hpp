#pragma once
#include "Details.hpp"
#include "AST.hpp"
#include "SymbolTable.hpp"

namespace Lauter::Conversion
{
	//Классификация без построения узла
	enum class ConversionKind
	{
		Invalid,   //невозможное преобразование
		Unchanged, //не нуждается в преобразовании (совпадает + модификаторы совместимы)
		Implicit,  //допустимо неявно
		Explicit,  //требует явного приведения
	};

	//Какой AST-узел нужно построить, если решили конвертировать.
	//Известно уже на этапе classify — не нужно повторно резолвить алиасы при build.
	enum class ConversionOperation
	{
		None,      // Unchanged/Invalid — строить нечего
		NumericCast,   // ConversionExpression
		InterfaceCast, // InterfaceCastExpression
	};

	struct Classification
	{
		ConversionKind kind = ConversionKind::Invalid;
		ConversionOperation operation = ConversionOperation::None;
	};


	
	class ConversionSystem
	{
	private:
		const BuiltinTypeCache& primitives;


		//Проверяет доступность изменения квалификаторов
		bool checkModifiers(const QualifiedType& from, const QualifiedType& to) const;

		//Проверяет, реализует ли objType интерфейс ifaceType
		bool implementsInterface(const SemanticType* objType, const SemanticType* ifaceType) const;

		inline int numericRankOf(const SemanticType* type) const
		{
			if (type == primitives.int8)  return 0;
			if (type == primitives.int16) return 1;
			if (type == primitives.int32) return 2;
			if (type == primitives.real32) return 3;
			if (type == primitives.real64) return 4;

			return -1;
		}
	public:
		explicit ConversionSystem(const BuiltinTypeCache& primitives)
			: primitives(primitives) 
		{}

		//Оценка преобразования
		Classification classify(const QualifiedType& from, const QualifiedType& to) const;

		//Вызывается только после classify() == Implicit/Explicit
		//Возвращает специальный AST узел преобразования типов
		AST::ExpressionPtr build(AST::ExpressionPtr expression, const Classification& classification, const QualifiedType& to) const;
	};
}