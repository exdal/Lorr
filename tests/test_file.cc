#include <gtest/gtest.h>

#include "Engine/OS/OS.hh"

constexpr static std::string_view str = "hello world!";
TEST(file, write_file) {
    auto path = fs::temp_directory_path() / "lr_test_file.txt";

    lr::File file(path, lr::FileAccess::Write);
    EXPECT_EQ(file.result, lr::FileResult::Success) << "Cannot open file to write";

    file.write(str.data(), { 0, str.length() });
    file.close();
}

TEST(file, read_file) {
    auto path = fs::temp_directory_path() / "lr_test_file.txt";

    lr::File file(path, lr::FileAccess::Read);
    EXPECT_EQ(file.result, lr::FileResult::Success) << "Cannot open file to read";
    EXPECT_EQ(file.size, str.length()) << "Read file size (" << file.size << ") is not same with `str`(" << str.length() << ")";

    auto file_data = file.whole_data();
    file.close();

    std::string_view read_str = { reinterpret_cast<const c8 *>(file_data.get()), file.size };
    EXPECT_TRUE(read_str == str) << "File content is not correct";
}
