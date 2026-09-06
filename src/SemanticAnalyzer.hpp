#pragma once
#include "AST.hpp"
#include "Visitor.hpp"
#include "Diagnostic.hpp"
#include "SymbolTable.hpp"
#include "ConversionSystem.hpp"

namespace Lauter
{
	class SemanticAnalyzer : public AST::ExpressionVisitor, public AST::StatementVisitor
	{
	private:
		DiagnosticEngine& diagnostics;
		ModuleRegistry& moduleRegistry;
		Conversion::ConversionSystem conversions;

		Module* currentModule = nullptr;

		
		void warning(AST::ASTNode& node, ReportCode code, const std::string& message);
		void error(AST::ASTNode& node, ReportCode code, const std::string& message);

		//Формирует сигнатуру функции из переданных параметров
		FunctionSignature buildCallSignature(std::vector<AST::ExpressionPtr>& arguments);

		//Макрос для методов, реализующих обход Expression-узлов
		#define X(name) QualifiedType visit(AST::name&) override;
		LAUTER_AST_EXPR_NODES(X)
		#undef X

		//Макрос для методов, реализующих обход Statement-узлов
		#define X(name) void visit(AST::name&) override;
		LAUTER_AST_STMT_NODES(X)
		#undef X

		void pushScope();
		void popScope();
		
		void openNamespace(const QualifiedName& namespaceName);
		void closeNamespace();

	public:
		SemanticAnalyzer(DiagnosticEngine& diagnostics, ModuleRegistry& moduleRegistry)
			: diagnostics(diagnostics), moduleRegistry(moduleRegistry), conversions(moduleRegistry.getCoreModule()) {
		}

		void analyze(Module& module, AST::TranslationUnit& translationUnit)
		{
			currentModule = &module;

			for (auto& stmt : translationUnit.statements)
				stmt->accept(*this);
		}

	
	};
}