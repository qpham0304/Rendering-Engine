#include <gtest/gtest.h>
#include "TestHelpers.h"
#include "core/features/ServiceLocator.h"
#include "logging/framework/LoggerSpd.h"

// The engine looks services up by name through ServiceLocator. Unit tests run without Engine,
// so this registers the minimum they need: the engine logger that Scene and EntitySerializer grab.
// Anything that needs GPU managers (textures, models, renderers) belongs in the smoke tests instead.
class EngineTestEnvironment : public ::testing::Environment {
public:
	void SetUp() override
	{
		ServiceLocator::setContext(&m_serviceLocator);
		m_logger = std::make_unique<LoggerSpd>("Engine");	// registers as "Engine_LoggerSPD"
		m_logger->setLevel(LogLevel::Warn);					// keep test output readable, errors still print
		m_serviceLocator.Register(m_logger->name(), *m_logger);
	}

	void TearDown() override
	{
		ServiceLocator::setContext(nullptr);
	}

private:
	ServiceLocator m_serviceLocator;
	std::unique_ptr<LoggerSpd> m_logger;
};

int main(int argc, char** argv)
{
	TestHelpers::disableCrashDialogs();
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new EngineTestEnvironment);
	return RUN_ALL_TESTS();
}
