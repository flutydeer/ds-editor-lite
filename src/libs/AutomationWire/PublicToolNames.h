#ifndef AUTOMATIONWIRE_PUBLICTOOLNAMES_H
#define AUTOMATIONWIRE_PUBLICTOOLNAMES_H

#include <QLatin1StringView>

namespace AutomationWire::PublicToolNames {

    // MCP tool names are protocol identifiers, distinct from editor operation IDs.
#define AUTOMATION_WIRE_PUBLIC_TOOL(symbol, tracking, name, category, profile, kind, sync, ...)    \
    inline constexpr QLatin1StringView symbol(name);
#include "PublicToolDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_TOOL

}

#endif // AUTOMATIONWIRE_PUBLICTOOLNAMES_H
