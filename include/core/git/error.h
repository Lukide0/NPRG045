#pragma once

#include <git2/errors.h>
#include <string>

namespace core::git {

inline std::string get_last_error() {
    const auto* err = git_error_last();
    if (err == nullptr || err->message == nullptr) {
        return "Unknown error";
    }

    return err->message;
}

}
