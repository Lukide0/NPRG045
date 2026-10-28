#pragma once

#include "core/action/ActionsManager.h"
#include <cstdint>

namespace core::rebase {

class RebaseStepView;

class RebasePlan {
public:
    using StepId = std::uint32_t;

private:
    action::ActionsManager m_actions;
};

class RebaseStepView { };

}
