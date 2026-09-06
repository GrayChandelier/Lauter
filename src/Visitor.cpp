#include "AST.hpp"
#include "Visitor.hpp"

namespace Lauter::AST
{
#define X(name) \
	QualifiedType name::accept(ExpressionVisitor& visitor) { return visitor.visit(*this); }
	LAUTER_AST_EXPR_NODES(X)
#undef X

#define X(name) \
	void name::accept(StatementVisitor& visitor) { visitor.visit(*this); }
		LAUTER_AST_STMT_NODES(X)
#undef X
}