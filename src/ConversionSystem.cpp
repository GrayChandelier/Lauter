#include "ConversionSystem.hpp"

namespace Lauter::Conversion
{
	//Проверяет доступность изменения квалификаторов
	bool ConversionSystem::checkModifiers(const QualifiedType& from, const QualifiedType& to) const
	{
		// ref нельзя неявно превратить в значение (потеря идентичности объекта).
		// Значение можно неявно заимствовать как ref (implicit borrow).
		if (from.isReference && !to.isReference)
			return false;

		// const нельзя неявно снять — но можно добавить (mut -> const безопасно).
		if (from.isConst && !to.isConst)
			return false;

		return true;
	}

	//Проверяет, реализует ли objType интерфейс ifaceType
	bool ConversionSystem::implementsInterface(const SemanticType* objType, const SemanticType* ifaceType) const
	{
		if (ifaceType->kind != TypeKind::Interface)
			return false;

		if (objType->kind == TypeKind::Interface)
			return objType == ifaceType; // нет наследования интерфейсов — только точное совпадение указателя

		if (objType->kind != TypeKind::Class)
			return false;

		const ClassType* cls = static_cast<const ClassType*>(objType);
		const InterfaceType* iface = static_cast<const InterfaceType*>(ifaceType);
		return cls->interfaces.contains(iface);
	}

	Classification ConversionSystem::classify(const QualifiedType& from, const QualifiedType& to) const
	{
		if (!from.type || !to.type)
			return { ConversionKind::Invalid, ConversionOperation::None };

		const SemanticType* fromResolved = resolveAlias(from.type);
		const SemanticType* toResolved = resolveAlias(to.type);

		if (!fromResolved || !toResolved)
			return { ConversionKind::Invalid, ConversionOperation::None }; // цикл псевдонимов

		if (!checkModifiers(from, to))
			return { ConversionKind::Invalid, ConversionOperation::None };

		if (fromResolved == toResolved)
			return { ConversionKind::Unchanged, ConversionOperation::None };

		if (fromResolved->kind == TypeKind::Primitive && toResolved->kind == TypeKind::Primitive)
		{


			//Преобразование в bool
			if (toResolved == primitives.boolType )
			{
				if(numericRankOf(fromResolved) == -1)
					return { ConversionKind::Invalid, ConversionOperation::None };

				return { ConversionKind::Implicit, ConversionOperation::NumericCast };
			}

			//Преобразование из bool не допускается
			if (fromResolved == primitives.boolType)
				return { ConversionKind::Invalid, ConversionOperation::None };


			bool widening = numericRankOf(fromResolved) < numericRankOf(toResolved);
			return { widening ? ConversionKind::Implicit : ConversionKind::Explicit, ConversionOperation::NumericCast };
		}

		if (implementsInterface(fromResolved, toResolved))
			return { ConversionKind::Implicit, ConversionOperation::InterfaceCast };

		return { ConversionKind::Invalid, ConversionOperation::None };
	}

	AST::ExpressionPtr ConversionSystem::build(AST::ExpressionPtr expression, const Classification& classification, const QualifiedType& to) const
	{
		assert(expression);

		assert(classification.kind == ConversionKind::Implicit || classification.kind == ConversionKind::Explicit);

		switch (classification.operation)
		{
		case ConversionOperation::NumericCast:
			return std::make_unique<AST::ConversionExpression>(std::move(expression), to);
		case ConversionOperation::InterfaceCast:
			return std::make_unique<AST::InterfaceCastExpression>(std::move(expression), to);
		default:
			assert(false && "ConversionSystem::build() was called for Unchanged/Invalid");
			return nullptr;
		}
	}
}