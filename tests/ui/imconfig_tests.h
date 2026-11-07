#pragma once
// an assertion of ImGui is a failure of the test of the window, not the end of the program
#include <stdexcept>
#define IM_ASSERT(e) ((e) ? (void)0 : throw std::runtime_error(#e))
