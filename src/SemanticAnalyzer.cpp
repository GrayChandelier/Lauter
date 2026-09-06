#include "SemanticAnalyzer.hpp"
#include "ConversionSystem.hpp"

namespace Lauter
{

	inline ReportLocation getReportLocation(Module* module, AST::ASTNode& node)
	{	
		assert(module);

		const auto& loc = node.getLocation();
		return ReportLocation{ .source = module->getModuleName(),
							   .column = loc.column, 
							   .row = loc.row };
	}

	void SemanticAnalyzer::warning(AST::ASTNode& node, ReportCode code, const std::string& message)
	{
		Report report;
		report.location = getReportLocation(currentModule, node);
		report.message = message;
		report.severity = Severity::Warning;
		report.code = code;

		diagnostics.report(std::move(report));
	}

	void SemanticAnalyzer::error(AST::ASTNode& node, ReportCode code, const std::string& message)
	{
		Report report;
		report.location = getReportLocation(currentModule, node);
		report.message = message;
		report.severity = Severity::Error;
		report.code = code;

		diagnostics.report(std::move(report));
	}

	FunctionSignature SemanticAnalyzer::buildCallSignature(std::vector<AST::ExpressionPtr>& arguments)
	{
		FunctionSignature sig;
		sig.parameters.reserve(arguments.size());

		//Рекурсивно типизируем каждый аргумент
		for (auto& arg : arguments)
			sig.parameters.push_back(arg->accept(*this));

		return sig;
	}

	void SemanticAnalyzer::pushScope()
	{
		currentModule->pushScope();
	}
	void SemanticAnalyzer::popScope()
	{
		currentModule->popScope();
	}
	void SemanticAnalyzer::openNamespace(const QualifiedName& namespaceName)
	{
		currentModule->openNestedNamespace(namespaceName);
	}
	void SemanticAnalyzer::closeNamespace()
	{
		currentModule->closeNamespace();
	}


	QualifiedType SemanticAnalyzer::visit(AST::Literal& node)
	{
		auto lookupPrimitive = [&](const char* name) -> QualifiedType
			{
				const Symbol* sym = moduleRegistry.getCoreModule()->lookup(QualifiedName{ {name} });
				assert(sym && sym->getKind() == SymbolKind::Type);

				const TypeSymbol* typeSymbol = static_cast<const TypeSymbol*>(sym);

				QualifiedType result;
				result.type = typeSymbol->getType();
				return result;
			};


		switch (node.kind)
		{
			case LiteralType::Int:     return lookupPrimitive("int32");
			case LiteralType::Real:    return lookupPrimitive("real64");
			case LiteralType::Boolean: return lookupPrimitive("bool");
		}

		error(node, ReportCode::UNKNOWN, "Unknown literal type");
		return QualifiedType{};
	}

	QualifiedType SemanticAnalyzer::visit(AST::BinaryExpression& node) 
	{
		//Получение типов операндов (handle-sides)
		QualifiedType lhs = node.leftOperand->accept(*this);
		QualifiedType rhs = node.rightOperand->accept(*this);

		//Ошибка уже сообщена глубже (например, неизвестный идентификатор) — не дублируем диагностику
		if (!lhs.type || !rhs.type)
			return QualifiedType{};

		//Разрешение псевдонимов
		const SemanticType* lhsResolved = resolveAlias(lhs.type);
		const SemanticType* rhsResolved = resolveAlias(rhs.type);

		//Проверка цикла псевдонимов
		if (!lhsResolved || !rhsResolved)
		{
			error(node, ReportCode::SemanticError, "Alias cycle detected: " + 
											  lhs.type->fullname.toString() + 
											  " and " + 
											  rhs.type->fullname.toString());
			return QualifiedType{};
		}

		//Если требуется преобразование
		if (lhsResolved != rhsResolved)
		{
			//Пробуем привести rhs к типу lhs
			auto rhsClass = conversions.classify(rhs, lhs);

			//Допускается неявное преобразование типа правого операнда к типу левого операнда
			if (rhsClass.kind == Conversion::ConversionKind::Implicit)
			{
				node.rightOperand = conversions.build(std::move(node.rightOperand), rhsClass, lhs);
				rhs = lhs;
			}
			else
			{
				//Проверка противоположного направления преобразования
				auto lhsClass = conversions.classify(lhs, rhs);

				//Допускается неявное преобразование типа левого операнда к типу правого операнда
				if (lhsClass.kind == Conversion::ConversionKind::Implicit)
				{
					node.leftOperand = conversions.build(std::move(node.leftOperand), lhsClass, rhs);
					lhs = rhs;
				}
				else
				{
					error(node, ReportCode::IncompatibleOperands,
						"IncompatibleOperands: " + lhs.type->fullname.toString() +
						" and " + rhs.type->fullname.toString());
					return QualifiedType{};
				}
			}
		}
	
		/*Поиск перегруженного оператора*/
		
		QualifiedName opName{ lhs.type->fullname.{ binaryOperatorFunctionName(node.op) } };
	}
	QualifiedType SemanticAnalyzer::visit(AST::UnaryExpression& node) 
	{
		return {};
	}
	QualifiedType SemanticAnalyzer::visit(AST::PostfixExpression& node) 
	{
		return {};
	}
	QualifiedType SemanticAnalyzer::visit(AST::IdentifierExpression& node) 
	{
		return {};
	}
	QualifiedType SemanticAnalyzer::visit(AST::ContainerLiteralExpression& node) 
	{
		return {};
	}
	QualifiedType SemanticAnalyzer::visit(AST::AssignmentExpression& node) 
	{
		return {};
	}
	QualifiedType SemanticAnalyzer::visit(AST::ErrorExpression& node) 
	{
		return {};
	}

	void SemanticAnalyzer::visit(AST::Block& node) {} 
	void SemanticAnalyzer::visit(AST::AmbiguousChain& node) {} 
	void SemanticAnalyzer::visit(AST::ExpressionStatement& node) {} 
	void SemanticAnalyzer::visit(AST::IfStatement& node) {} 
	void SemanticAnalyzer::visit(AST::WhileStatement& node) {} 
	void SemanticAnalyzer::visit(AST::ForInStatement& node) {} 
	void SemanticAnalyzer::visit(AST::ContinueStatement& node) {} 
	void SemanticAnalyzer::visit(AST::BreakStatement& node) {} 
	void SemanticAnalyzer::visit(AST::ReturnStatement& node) {} 
	void SemanticAnalyzer::visit(AST::WhenStatement& node) {} 
	void SemanticAnalyzer::visit(AST::DeclarationStatement& node) {} 
	void SemanticAnalyzer::visit(AST::NamespaceDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::FuncDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::ConstructorDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::DestructorDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::ClassDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::InterfaceDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::ImportDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::RuntimeHookDeclaration& node) {} 
	void SemanticAnalyzer::visit(AST::ErrorStatement& node) {} 
	void SemanticAnalyzer::visit(AST::ErrorDeclaration& node) {}
}