#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <string>

#include "utils/file.h"

TEST_CASE("file read/write/copy and scope limit behavior")
{
    namespace fs = std::filesystem;

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp_dir = fs::temp_directory_path() / ("subconverter_ut_" + std::to_string(now));
    fs::create_directories(temp_dir);

    const fs::path file_path = temp_dir / "input.txt";
    const fs::path copy_path = temp_dir / "copy.txt";
    const std::string abs_file = file_path.string();
    const std::string abs_copy = copy_path.string();

    REQUIRE(fileWrite(abs_file, "hello", true) == 0);
    CHECK(fileExist(abs_file, false));
    CHECK(fileGet(abs_file, false) == "hello");

    REQUIRE(fileWrite(abs_file, "-tail", false) == 0);
    CHECK(fileGet(abs_file, false) == "hello-tail");

    CHECK_FALSE(fileExist(abs_file, true));
    CHECK(fileGet(abs_file, true).empty());

    CHECK(fileCopy(abs_file, abs_copy));
    CHECK(fileGet(abs_copy, false) == "hello-tail");

    fs::remove_all(temp_dir);
}
