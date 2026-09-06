#pragma once
#include <string>
#include <vector>
#include <stdexcept>

namespace Lauter
{

	enum class Severity : uint8_t { Unknown, Note, Warning, Error };

	enum class ReportCode : uint16_t
	{
		UNKNOWN = 0,
		ParsingError = 200,
		UnexpectedToken = 201,
		BreakOutsideLoop = 202,
		ContinueOutsideLoop = 203,
		DefaultCaseMustBeLast = 204,
		EmptyImportList = 206,
		EmptyInterface = 207,
		RedefiningDestructor = 208,

		SemanticError = 300,
		UnknownNamespace = 301,
		UndefinedSymbol = 302,
		IncompatibleOperands = 303

	};

	struct ReportLocation
	{
		std::string source;
		size_t column = 0;
		size_t row = 0;
	};

	struct Report
	{
		Severity severity = Severity::Unknown;

		ReportLocation location;

		std::string message;
		ReportCode code = ReportCode::UNKNOWN;
	};


	class DiagnosticEngine
	{
	private:
		size_t errors = 0;
	

		std::vector<Report> reports;

		inline void severityCheck(Severity severity)
		{
			if (severity == Severity::Error)
				errors++;

		}
	public:
		
		void report(Report&& report)
		{	
			reports.push_back(std::move(report));
			severityCheck(report.severity);
		}

		const auto& viewReports() const noexcept
		{
			return reports;
		}

		inline bool hasErrors() const noexcept
		{
			return errors > 0;
		}

		inline size_t getErrorCount() const noexcept
		{
			return errors;
		}
		
	};
}