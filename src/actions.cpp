#include "server/actions.hpp"

namespace app {

// ── 统一动作注册入口（表驱动）──────────────────────────────────────
const std::unordered_map<std::string, ActionHandler>& action_registry() {
    static const std::unordered_map<std::string, ActionHandler> registry = {
        {"list_items",   &handle_list_items},
        {"create_item",  &handle_create_item},
        {"update_item",  &handle_update_item},
        {"delete_item",  &handle_delete_item},
    };
    return registry;
}

ActionHandler find_action(const std::string& action_name) {
    const auto& registry = action_registry();
    const auto it = registry.find(action_name);
    return it != registry.end() ? it->second : nullptr;
}

} // namespace app
