#include <fstream> //read lauter sorce files
#include <filesystem>

#include "Lexer.hpp" //lauter lexer
#include "Parser.hpp" //Синтаксический анализатор
#include "SymbolTable.hpp" //Система модулей и таблица символов
#include "SemanticAnalyzer.hpp" //Семантический анализатор
#include "ConversionSystem.hpp" //Система преобразования типов

int checkErrors(Lauter::DiagnosticEngine& diagnosticEngine)
{
	size_t errorCount = 0;
	size_t warningCount = 0;
	for (const auto& report : diagnosticEngine.viewReports())
	{
		switch (report.severity)
		{
		case Lauter::Severity::Error:
		{
			std::cout << "[ERROR] "; errorCount++;
			break;
		}
		case Lauter::Severity::Warning:
		{
			std::cout << "[WARNING] "; warningCount++;
			break;
		}
		default: std::cout << "[NOTE] ";
		}

		std::cout << report.message
			<< " at "
			<< std::to_string(report.location.row)
			<< ":"
			<< std::to_string(report.location.column)
			<< " ("
			<< std::to_string(static_cast<int>(report.code))
			<< ")\n";
	}
	
	std::cout << "Errors: " << errorCount << ", Warnings: " << warningCount << "\n";

	if (errorCount)
	{
		std::cout << "Compilation failed.\n";
		return -1;
	}

	std::cout << "Compilation finished successfully!\n";
	return 0;
}

int main(int argc, char* argv[])
{
	std::filesystem::path source("..\\tests\\baseTest.laut");
	std::ifstream file(source);

	if (!file.is_open())
	{
		std::cerr << "Failed to open translation unit source code\n";
		return -1;
	}
	//Сборщик ошибок компиляции
	Lauter::DiagnosticEngine diagnosticEngine;

	//Регистр модулей
	Lauter::ModuleRegistry moduleRegistry;

	//Лексер
	Lauter::Lexer lexer;
	auto tokens = lexer.extractTokens(file);

	//Интерфейс для чтения потока токенов
	Lauter::TokenStreamReader tokenReader(tokens);

	//Система преобразования типов
	//Lauter::ConversionSystem conversions(*moduleRegistry.getCoreModule());

	//Синтаксический анализатор
	Lauter::Parser parser(tokenReader, diagnosticEngine);
	Lauter::AST::TranslationUnit astRoot = parser.parse();

	//Семантический анализатор
	Lauter::SemanticAnalyzer semanticAnalyzer(diagnosticEngine, moduleRegistry);

	//абсолютный путь исключает дубликаты
	Lauter::Module* module = moduleRegistry.createModule(std::filesystem::absolute(source).string());

	//Обработка AST
	semanticAnalyzer.analyze(*module, astRoot);
	
	//Вывод результата компиляции
	if (!checkErrors(diagnosticEngine)) 
		return -2;

	/*Оптимизатор и кодогенератор в процессе*/

	return 0;
}
