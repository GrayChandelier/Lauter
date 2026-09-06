#pragma once
#include <unordered_map>
#include <memory>
#include <deque>
#include <cassert>
#include <unordered_set>

#include "AST.hpp"
#include "Diagnostic.hpp"
#include "TypeRegistry.hpp"

namespace Lauter
{
	class NamespaceError : public std::runtime_error 
	{
	public:
		int badPartIndex = -1;
		explicit NamespaceError(const std::string& message, int badPartIndex)
			: std::runtime_error(message), badPartIndex(badPartIndex) {}
	};


	class Symbol
	{
	private:	
		std::string name;
		SymbolKind kind;
	public:
		Symbol(std::string name, SymbolKind kind)
			: name(std::move(name)), kind(kind) 
		{ }

		SymbolKind getKind() const
		{
			return kind;
		}

		const std::string& getName() const
		{
			return name;
		}
		virtual ~Symbol() = default;

	};
	using SymbolPtr = std::unique_ptr<Symbol>;



	//Переменная или объект
	class VariableSymbol : public Symbol
	{
	private:
		QualifiedType type;
		bool inited = false;
	public:
		VariableSymbol(std::string name, QualifiedType type)
			: Symbol(std::move(name), SymbolKind::Variable), type(std::move(type))
		{}

		inline const QualifiedType& getQualifiedType() const
		{
			return type;
		}

		inline void initialize()
		{
			inited = true;
		}
		inline bool isInitialized() const
		{
			return inited;
		}
	};

	//Пользовательский тип или шаблонный параметр
	class TypeSymbol : public Symbol
	{
	private:
		SemanticType* type = nullptr;
	public:
		TypeSymbol(std::string name,SemanticType* type)
			: Symbol(std::move(name), SymbolKind::Type), type(type)
		{}

		inline const SemanticType* getType() const
		{
			return type;
		}
	};


	class FunctionOverload
	{
	public:

		enum class FunctionState
		{
			Declared,
			Defined,
			Extern //внешняя функция, не требует блока с реализацией
		};

		explicit FunctionOverload(FunctionType* type)
			: type(type)
		{
			assert(type);
		}

		FunctionState getFunctionState() const
		{
			return state;
		}

		inline bool isDefined() const
		{
			return state == FunctionState::Defined;
		}
		inline void define()
		{
			state = FunctionState::Defined;
		}

		const FunctionType* getFunctionType() const
		{
			return type;
		}
	private:
		FunctionType* type;
		FunctionState state = FunctionState::Declared;
	};

	class FunctionSet : public Symbol
	{
	private:
		std::vector<FunctionOverload> overloads;
	public:

		explicit FunctionSet(std::string name)
			: Symbol(std::move(name), SymbolKind::FunctionOverloads)
		{
		}

		//const FunctionOverload* lookupOverload(const FunctionSignature& signature) const
		//{
		//	for (const auto& item : overloads)
		//	{
		//		if (item.getFunctionType()->signature.sameParameters(signature))
		//			return &item;
		//	}
		//
		//	return nullptr;
		//}

		//bool tryInsert(FunctionType* type)
		//{
		//	if (lookupOverload(type->signature))
		//		return false;
		//
		//	overloads.emplace_back(type);
		//	return true;
		//}

	};

	class GenericParameterSymbol : public Symbol
	{
	private:
		GenericParameterType* type;

	public:
		GenericParameterSymbol(std::string name, GenericParameterType* type)
			: Symbol(std::move(name), SymbolKind::GenericParameter), type(type)
		{ }

		GenericParameterType* getType()
		{
			return type;
		}
	};


	//Область видимости {}
	class Scope
	{
	private:
		using SymbolPtr = std::unique_ptr<Symbol>;
		std::unordered_map<std::string, SymbolPtr> symbols;

	public:

		const Symbol* lookupSymbol(const std::string& name) const
		{
			auto it = symbols.find(name);

			if (it == symbols.end())
				return nullptr;

			return it->second.get();
		}
		Symbol* insertSymbol(std::unique_ptr<Symbol> symbol)
		{
			auto [it, inserted] = symbols.try_emplace(symbol->getName(), std::move(symbol));
			if (!inserted) return nullptr;
			return it->second.get();
		}
	};

	class ScopeManager
	{
	private:
		std::deque<Scope> stack;
	public:
		ScopeManager()
		{
			//global scope
			pushScope(); 
		}
		void pushScope()
		{
			stack.emplace_back();
		}
		void popScope()
		{
			assert(stack.size() > 1);
			stack.pop_back();
		}

		//Ищет символ среди всех scopes
		const Symbol* lookupSymbol(const std::string& name) const
		{
			for (size_t i = stack.size(); i-- > 0;)
			{
				if (auto symbol = stack[i].lookupSymbol(name))
					return symbol;
			}
			return nullptr;
		}

		//Вставляет символ в последний scope
		Symbol* insertSymbol(std::unique_ptr<Symbol> symbol)
		{
			return getLastScope().insertSymbol(std::move(symbol));
		}

		//Глобальный scope
		Scope& getFirstScope()
		{
			assert(!stack.empty());
			return stack.front();
		}

		//Метод для поиска символов в локальном scope
		Scope& getLastScope()
		{
			assert(!stack.empty());
			return stack.back();
		}

		//Метод для поиска символов в локальном scope
		const Scope& getLastScope() const
		{
			assert(!stack.empty());
			return stack.back();
		}

		//Глобальный scope
		const Scope& getFirstScope() const
		{
			assert(!stack.empty());
			return stack.front();
		}
	};

	class Namespace
	{
	private:
		using NamespacePtr = std::unique_ptr<Namespace>;

		Namespace* parent = nullptr;

		//Дочерние пространства имён
		std::unordered_map<std::string, NamespacePtr> children;

		//Смонтированные пространства имён из других модулей
		std::unordered_map<std::string, Namespace*> mounted;

		Namespace* lookupMountedNamespace(const std::string& name) const
		{
			auto it = mounted.find(name);

			if (it == mounted.end())
				return nullptr;

			return it->second;
		}

		bool contains(const std::string& name) const
		{
			return children.contains(name) || mounted.contains(name);
		}
	public:
		Namespace(Namespace* parent)
			: parent(parent) { }

		Namespace* getParent() const
		{
			return parent;
		}
		//Монтирует внешнее пространство имён
		bool tryMountNamespace(const std::string& alias, Namespace* ns)
		{
			//Запрет на монтирование несуществующего namespace
			if (!ns)
				return false;

			//Запрет на дублирование псевдонима
			if (contains(alias))
				return false;

			mounted[alias] = ns;
			return true;
		}

		//Создаёт дочернее пространство имён
		Namespace* tryCreateChildNamespace(const std::string& name)
		{
			if (contains(name))
				return nullptr;

			auto [it, inserted] =
				children.emplace(name, std::make_unique<Namespace>(this));

			return it->second.get();
		}

		//Ищет дочернее или смонтированное пространство имён
		Namespace* lookupChildNamespace(const std::string& name) const
		{
			if (auto it = children.find(name); it != children.end())
				return it->second.get();

			return lookupMountedNamespace(name);
		}

		//Области видимости данного пространства имён
		ScopeManager scopeManager;
	};

	using ModulePath = std::string;
	using ModuleAlias = std::string;


	class Module
	{
	private:
		ModulePath sourcePath;
		Module* coreModule = nullptr;

		Namespace* currentNamespace = nullptr;

	public:
		Module(Module* coreModule, ModulePath sourcePath)
			: sourcePath(std::move(sourcePath)),
			  coreModule(coreModule),
			  globalNamespace(nullptr),
			  currentNamespace(&globalNamespace)
		{}

		//Объявленные типы
		TypeRegistry typeRegistry;

		//Дерево пространств имён
		Namespace globalNamespace;

		const std::string& getModuleName() const
		{
			return sourcePath;
		}
		//Находит namespace по QualifiedName символа
		//Если хоть один элемент цепи namespaces не существует, возвращает nullptr и записывает индекс имени этого namespace в unknownPartIndex
		const Namespace* lookupNamespace(const QualifiedName& symbolName, size_t& unknownPartIndex) const
		{
			const auto& parts = symbolName.parts;

			if (parts.size() == 1)
				return &globalNamespace;

			const Namespace* head = &globalNamespace;

			
			for (size_t partIndex = 0; partIndex < parts.size() - 1; partIndex++)
			{
				head = head->lookupChildNamespace(parts[partIndex]);

				if (!head)
				{
					unknownPartIndex = partIndex;
					return nullptr;
				}
			}

			return head;
		}

		const Symbol* lookup(const QualifiedName& symbolName) const
		{
			assert(symbolName.parts.size() > 0);

			size_t unknownPartIndex = 0;
			const Namespace* ns = lookupNamespace(symbolName, unknownPartIndex);

			if (!ns) throw NamespaceError("Unknown namespace", unknownPartIndex);

			const Symbol* symbol = ns->scopeManager.lookupSymbol(symbolName.shortName());
			if (symbol) return symbol;

			if (coreModule) return coreModule->lookup(symbolName);

			return nullptr;
		}

		const VariableSymbol* lookupVariable(const QualifiedName& name) const
		{
			const Symbol* sym = lookup(name);

			if (!sym || sym->getKind() != SymbolKind::Variable)
				return nullptr;

			return static_cast<const VariableSymbol*>(sym);
		}

		const FunctionSet* lookupFunctionSet(const QualifiedName& name) const
		{
			const Symbol* sym = lookup(name);

			if (!sym || sym->getKind() != SymbolKind::FunctionOverloads)
				return nullptr;

			return static_cast<const FunctionSet*>(sym);
		}

		const FunctionOverload* lookupFunctionOverload(const QualifiedName& name, const FunctionSignature& callSignature) const
		{
			const FunctionSet* funcSet = lookupFunctionSet(name);

			if (!funcSet) 
				return nullptr;

			return nullptr;
		//	return funcSet->lookupOverload(callSignature);
		}

		const TypeSymbol* lookupType(const QualifiedName& name) const
		{
			const Symbol* sym = lookup(name);

			if (!sym || sym->getKind() != SymbolKind::Type)
				return nullptr;

			return static_cast<const TypeSymbol*>(sym);
		}

		const InterfaceType* lookupInterface(const QualifiedName& name) const
		{
			const TypeSymbol* typeSym = lookupType(name);
			if (!typeSym) return nullptr;

			const SemanticType* resolved = resolveAlias(typeSym->getType());

			if (!resolved || resolved->kind != TypeKind::Interface)
				return nullptr;

			return static_cast<const InterfaceType*>(resolved);
		}

		void pushScope()
		{
			currentNamespace->scopeManager.pushScope();
		}
		void popScope()
		{
			currentNamespace->scopeManager.popScope();
		}
		void openNestedNamespace(const QualifiedName& namespaceName)
		{
			Namespace* ns = currentNamespace;
			size_t partIndex = 0;
			while (partIndex < namespaceName.parts.size())
			{
				const auto& part = namespaceName.parts[partIndex++];

				//Ищем существующее дочернее пространство имён
				ns = ns->lookupChildNamespace(part);

				//Или создаём новое
				if (!ns) ns = ns->tryCreateChildNamespace(part);

				//Присвоение валидного значения гарантируется одним из методов выше
				assert(ns);
			}

			currentNamespace = ns;
		}
		void closeNamespace()
		{
			assert(currentNamespace != &globalNamespace);
			currentNamespace = currentNamespace->getParent();
		}
	};





	class ModuleRegistry 
	{
	private:
		using ModulePtr = std::unique_ptr<Module>;

		//Модули
		std::unordered_map<ModulePath, ModulePtr> modules;

		//Модуль-ядро, предоставляющий базовые типы (int, bool, string)
		std::unique_ptr<Module> coreModule;

		BuiltinTypeCache builtins;
	public:
		const BuiltinTypeCache& getBuiltins() const
		{
			return builtins;
		}

		ModuleRegistry()
		{
			coreModule = std::make_unique<Module>(nullptr, "$core");
			TypeRegistry& builtinTypes = coreModule->typeRegistry;
	
			//Регистрация типа
			auto registerType = [&](QualifiedName name, size_t size, size_t alignment) -> PrimitiveType*
				{
					PrimitiveType* type = builtinTypes.create<PrimitiveType>(name, size, alignment);
					auto symbolType = std::make_unique<TypeSymbol>(type->fullname.shortName(), type);
					coreModule->globalNamespace
						.scopeManager
						.getFirstScope()
						.insertSymbol(std::move(symbolType));
					return type;
				};

			//Регистрация псевдонима типа
			auto registerAlias = [&](QualifiedName name, SemanticType* target)
				{
					AliasType* alias = builtinTypes.create<AliasType>(name, target);
					auto symbolType = std::make_unique<TypeSymbol>(alias->fullname.shortName(), alias);
					coreModule->globalNamespace
						.scopeManager
						.getFirstScope()
						.insertSymbol(std::move(symbolType));
				};


			//Целочисленный тип
			builtins.int8 = registerType(QualifiedName{ {"int8"} }, 1, 1);
			builtins.int16 = registerType(QualifiedName{ {"int16"} }, 2, 2);
			builtins.int32 = registerType(QualifiedName{ {"int32"} }, 4, 4);

			//Вещественный тип
			builtins.real64 = registerType(QualifiedName{ {"real64"} }, 8, 8);
			builtins.real32 = registerType(QualifiedName{ {"real32"} }, 4, 4);

			//Прочие типы
			builtins.boolType = registerType(QualifiedName{ {"bool"} }, 1, 1);
			builtins.voidType = registerType(QualifiedName{ {"void"} }, 0, 0);

			//Псевдонимы
			registerAlias(QualifiedName{ {"int"} }, builtins.int32);
			registerAlias(QualifiedName{ {"real"} }, builtins.real64);

			//Более привычные имена для вещественного типа
			registerAlias(QualifiedName{ {"float"} }, builtins.real32);
			registerAlias(QualifiedName{ {"double"} }, builtins.real64);
		}

		
		
		const Module* getCoreModule() const
		{
			return coreModule.get();
		}

		Module* createModule(std::string modulePath)
		{
			auto module = std::make_unique<Module>(coreModule.get(), modulePath);

			Module* result = module.get();

			auto [it, inserted] =
				modules.emplace(modulePath, std::move(module));

			if (!inserted)
				return nullptr;

			return result;
		}

		const Module* lookupModule(const std::string& moduleAbsolutePath) const
		{
			auto it = modules.find(moduleAbsolutePath);

			if (it == modules.end())
				return nullptr;

			return it->second.get();
		}
	};
}