#pragma once

#include <cassert>
#include <git2/types.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace git {

class CommentResult {
public:
    enum class State {
        Ok,
        FailedOpenConfig,
        FailedReadConfig,
        Empty,
        Deprecated,
    };

    CommentResult(std::string value, State state)
        : m_value(std::move(value))
        , m_state(state) { }

    [[nodiscard]] bool has_value() const { return m_state == State::Ok; }

    [[nodiscard]] const std::string& err_message() const {
        assert(!has_value());
        return m_value;
    }

    [[nodiscard]] State state() const { return m_state; }

    [[nodiscard]] const std::string& value() const {
        assert(has_value());
        return m_value;
    }

private:
    std::string m_value;
    State m_state;
};

CommentResult get_comment_string(git_repository* repo);

}
