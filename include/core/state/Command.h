#pragma once

namespace core::state {

class Command {
public:
    virtual ~Command()     = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;
};

}
