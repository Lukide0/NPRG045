#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "git/parser.h"
#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

using namespace git;

TEST_CASE("parse_stream returns no error for an empty stream") {
    std::istringstream input("");

    const auto result = parse_stream(input, "#");

    CHECK_FALSE(result.has_error());
    CHECK(result.err.empty());
    CHECK(result.actions.empty());
}

TEST_CASE("parse_stream ignores blank lines, comments and break commands") {
    std::istringstream input(
        "# This is a comment\n"
        "\n"
        "break\n"
        "pick abc123 Add a feature\n"
        "# Another comment\n"
        "edit def456 Fix a bug\n"
    );

    const auto result = parse_stream(input, "#");

    REQUIRE_FALSE(result.has_error());
    REQUIRE(result.actions.size() == 2);

    CHECK(result.actions[0].type == CmdType::PICK);
    CHECK(result.actions[0].hash == "abc123");
    CHECK(result.actions[0].line == 4);

    CHECK(result.actions[1].type == CmdType::EDIT);
    CHECK(result.actions[1].hash == "def456");
    CHECK(result.actions[1].line == 6);
}

TEST_CASE("parse_stream parses all supported commit commands") {
    std::istringstream input(
        "pick a111 First commit\n"
        "reword b222 Change commit message\n"
        "edit c333 Edit commit\n"
        "squash d444 Squash commit\n"
        "drop e555 Drop commit\n"
        "fixup f666 Fixup commit\n"
    );

    const auto result = parse_stream(input, "#");

    REQUIRE_FALSE(result.has_error());
    REQUIRE(result.actions.size() == 6);

    CHECK(result.actions[0].type == CmdType::PICK);
    CHECK(result.actions[0].hash == "a111");
    CHECK(result.actions[0].line == 1);

    CHECK(result.actions[1].type == CmdType::REWORD);
    CHECK(result.actions[1].hash == "b222");
    CHECK(result.actions[1].line == 2);

    CHECK(result.actions[2].type == CmdType::EDIT);
    CHECK(result.actions[2].hash == "c333");
    CHECK(result.actions[2].line == 3);

    CHECK(result.actions[3].type == CmdType::SQUASH);
    CHECK(result.actions[3].hash == "d444");
    CHECK(result.actions[3].line == 4);

    CHECK(result.actions[4].type == CmdType::DROP);
    CHECK(result.actions[4].hash == "e555");
    CHECK(result.actions[4].line == 5);

    CHECK(result.actions[5].type == CmdType::FIXUP);
    CHECK(result.actions[5].hash == "f666");
    CHECK(result.actions[5].line == 6);
}

TEST_CASE("parse_stream handles fixup commit flags") {
    std::istringstream input(
        "fixup -C abc123 Reuse commit message\n"
        "fixup -c def456 Edit commit message\n"
    );

    const auto result = parse_stream(input, "#");

    REQUIRE_FALSE(result.has_error());
    REQUIRE(result.actions.size() == 2);

    CHECK(result.actions[0].type == CmdType::FIXUP);
    CHECK(result.actions[0].hash == "abc123");
    CHECK(result.actions[0].line == 1);

    CHECK(result.actions[1].type == CmdType::FIXUP);
    CHECK(result.actions[1].hash == "def456");
    CHECK(result.actions[1].line == 2);
}

TEST_CASE("parse_stream accepts a custom comment character") {
    std::istringstream input(
        "; Generated comment\n"
        "pick abc123 Add a feature\n"
    );

    const auto result = parse_stream(input, ";");

    REQUIRE_FALSE(result.has_error());
    REQUIRE(result.actions.size() == 1);

    CHECK(result.actions[0].type == CmdType::PICK);
    CHECK(result.actions[0].hash == "abc123");
    CHECK(result.actions[0].line == 2);
}

TEST_CASE("parse_stream reports an error for an unknown command") {
    std::istringstream input(
        "pick abc123 Valid commit\n"
        "unknown def456 Invalid command\n"
        "pick ghi789 Another commit\n"
    );

    const auto result = parse_stream(input, "#");

    CHECK(result.has_error());
    CHECK(
        result.err
        == "Unknown command found in git rebase TODO file. The file may be "
           "corrupted or use unsupported rebase actions."
    );

    // The parser stops at the first error.
    REQUIRE(result.actions.size() == 1);
    CHECK(result.actions[0].hash == "abc123");
}

TEST_CASE("parse_stream reports an error when the commit hash is missing") {
    std::istringstream input("pick\n");

    const auto result = parse_stream(input, "#");

    CHECK(result.has_error());
    CHECK(result.err == "Failed to extract commit hash");
    CHECK(result.actions.empty());
}

TEST_CASE("parse_stream reports an error for unsupported commands") {
    struct UnsupportedCase {
        const char* command;
        const char* expected_error;
    };

    const std::vector<UnsupportedCase> cases {
        {
         .command        = "exec echo hello",
         .expected_error = "Execution commands (exec) are not supported. "
                              "Please remove or edit this command manually.", },
        {
         .command        = "update-ref refs/heads/test",
         .expected_error = "Update ref commands (update-ref) are not supported. "
                              "Please remove or edit this command manually.", },
        {
         .command        = "label my-label",
         .expected_error = "Label commands (label) are not supported. "
                              "Please remove or edit this command manually.", },
        {
         .command        = "reset my-label",
         .expected_error = "Reset commands (reset) are not supported. "
                              "Please remove or edit this command manually.", },
        {
         .command        = "merge -C abc123 my-branch",
         .expected_error = "Merge commands (merge) are not supported. "
                              "Please remove or edit this command manually.", },
    };

    for (const auto& test_case : cases) {
        INFO("Testing command: " << test_case.command);

        std::istringstream input(test_case.command);
        const auto result = parse_stream(input, "#");

        CHECK(result.has_error());
        CHECK(result.err == test_case.expected_error);
        CHECK(result.actions.empty());
    }
}

TEST_CASE("parse_stream reports a missing hash after a fixup flag") {
    std::istringstream input("fixup -C\n");

    const auto result = parse_stream(input, "#");

    CHECK(result.has_error());
    CHECK(result.err == "Failed to extract commit hash");
    CHECK(result.actions.empty());
}
