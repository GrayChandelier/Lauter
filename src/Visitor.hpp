#pragma once
#include "AST.hpp"

namespace Lauter::AST
{
	class ExpressionVisitor
	{
	public:
		virtual ~ExpressionVisitor() = default;

#define X(name) virtual QualifiedType visit(name&) = 0;
		LAUTER_AST_EXPR_NODES(X)
#undef X
	};

	class StatementVisitor
	{
	public:
		virtual ~StatementVisitor() = default;

#define X(name) virtual void visit(name&) = 0;
		LAUTER_AST_STMT_NODES(X)
#undef X
	};
}