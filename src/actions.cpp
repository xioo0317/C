#include "server/actions.hpp"

namespace app {

// ── 动作注册（POST /out 分发）───────────────────────────────────────
const std::unordered_map<std::string, Handler>& action_registry() {
    static const std::unordered_map<std::string, Handler> registry = {
        {"list_items",   &handle_list_items},
        {"create_item",  &handle_create_item},
        {"update_item",  &handle_update_item},
        {"delete_item",  &handle_delete_item},
    };
    return registry;
}

Handler find_action(const std::string& name) {
    const auto& registry = action_registry();
    const auto it = registry.find(name);
    return it != registry.end() ? it->second : nullptr;
}

} // namespace app
