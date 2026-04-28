#pragma once

namespace core::state {

/**
 * @brief Base interface for executable commands with undo support.
 */
class Command {
public:
    virtual ~Command()     = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;
};

}
