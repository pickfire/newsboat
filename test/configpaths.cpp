#include "configpaths.h"

#include "3rd-party/catch.hpp"
#include "test-helpers.h"
#include "utils.h"

using namespace newsboat;

TEST_CASE("ConfigPaths returns paths to Newsboat dotdir if no Newsboat dirs "
		"exist",
		"[ConfigPaths]")
{
	const auto test_dir = std::string("data/configpaths--no-dirs");
	const auto newsboat_dir = test_dir + "/.newsboat";

	TestHelpers::EnvVar home("HOME");
	home.set(test_dir);

	ConfigPaths paths;
	REQUIRE(paths.initialized());
	REQUIRE(paths.url_file() == newsboat_dir + "/urls");
	REQUIRE(paths.cache_file() == newsboat_dir + "/cache.db");
	REQUIRE(paths.lock_file() == newsboat_dir + "/cache.db.lock");
	REQUIRE(paths.config_file() == newsboat_dir + "/config");
	REQUIRE(paths.queue_file() == newsboat_dir + "/queue");
	REQUIRE(paths.search_file() == newsboat_dir + "/history.search");
	REQUIRE(paths.cmdline_file() == newsboat_dir + "/history.cmdline");
}

TEST_CASE("ConfigPaths returns paths to Newsboat XDG dirs if they exist and "
		"the dotdir doesn't",
		"[ConfigPaths]")
{
	const auto test_dir = std::string("data/configpaths--only-xdg");
	const auto config_dir = test_dir + "/.config/newsboat";
	const auto data_dir = test_dir + "/.local/share/newsboat";

	TestHelpers::EnvVar home("HOME");
	home.set(test_dir);

	const auto check = [&]() {
		ConfigPaths paths;
		REQUIRE(paths.initialized());
		REQUIRE(paths.config_file() == config_dir + "/config");
		REQUIRE(paths.url_file() == config_dir + "/urls");
		REQUIRE(paths.cache_file() == data_dir + "/cache.db");
		REQUIRE(paths.lock_file() == data_dir + "/cache.db.lock");
		REQUIRE(paths.queue_file() == data_dir + "/queue");
		REQUIRE(paths.search_file() == data_dir + "/history.search");
		REQUIRE(paths.cmdline_file() == data_dir + "/history.cmdline");
	};

	SECTION("XDG_CONFIG_HOME is set") {
		TestHelpers::EnvVar xdg_config("XDG_CONFIG_HOME");
		xdg_config.set(test_dir + "/.config");

		SECTION("XDG_DATA_HOME is set") {
			TestHelpers::EnvVar xdg_data("XDG_DATA_HOME");
			xdg_data.set(test_dir + "/.local/share");
			check();
		}

		SECTION("XDG_DATA_HOME is not set") {
			TestHelpers::EnvVar xdg_data("XDG_DATA_HOME");
			xdg_data.unset();
			check();
		}
	}

	SECTION("XDG_CONFIG_HOME is not set") {
		TestHelpers::EnvVar xdg_config("XDG_CONFIG_HOME");
		xdg_config.unset();

		SECTION("XDG_DATA_HOME is set") {
			TestHelpers::EnvVar xdg_data("XDG_DATA_HOME");
			xdg_data.set(test_dir + "/.local/share");
			check();
		}

		SECTION("XDG_DATA_HOME is not set") {
			TestHelpers::EnvVar xdg_data("XDG_DATA_HOME");
			xdg_data.unset();
			check();
		}
	}
}
