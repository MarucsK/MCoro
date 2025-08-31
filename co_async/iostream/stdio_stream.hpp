#pragma once
#include <co_async/std.hpp>
#include <co_async/iostream/stream_base.hpp>

namespace Marcus {

OwningStream &stdio();

OwningStream &raw_stdio();

} // namespace Marcus
