#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <cstdlib>
#endif

namespace TestHelpers {

// Debug asserts and abort() pop a modal dialog on Windows, which hangs Test Explorer.
// Send them to stderr instead so a failing assert just fails the run.
inline void disableCrashDialogs()
{
#ifdef _MSC_VER
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
}

// fresh folder per test under the system temp dir, removed when the test ends
class TempDir {
public:
	TempDir()
	{
		const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
		std::string name = info ? std::string(info->test_suite_name()) + "_" + info->name() : "EngineTests";
		m_path = std::filesystem::temp_directory_path() / "MyGraphicsEngineTests" / name;
		std::filesystem::remove_all(m_path);
		std::filesystem::create_directories(m_path);
	}

	~TempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(m_path, ec);
	}

	const std::filesystem::path& path() const { return m_path; }

	// writes text to <tempdir>/<fileName> and returns the full path as a string, ready for the engine loaders
	std::string write(std::string_view fileName, std::string_view contents) const
	{
		std::filesystem::path file = m_path / fileName;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::out | std::ios::trunc);
		out << contents;
		return file.string();
	}

	std::string file(std::string_view fileName) const
	{
		return (m_path / fileName).string();
	}

private:
	std::filesystem::path m_path;
};

}
