/*
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zmk/keymap.h>

#include <roamyboard/keymap_binding.h>

bool roamyboard_binding_is(const struct zmk_behavior_binding *binding, const char *behavior) {
    return binding->behavior_dev != NULL && strcmp(binding->behavior_dev, behavior) == 0;
}

const struct zmk_behavior_binding *roamyboard_keymap_binding_at(uint32_t position) {
    const zmk_keymap_layer_id_t base = zmk_keymap_layer_default();
    for (int index = ZMK_KEYMAP_LAYERS_LEN - 1; index >= 0; index--) {
        const zmk_keymap_layer_id_t layer = zmk_keymap_layer_index_to_id(index);
        if (layer == ZMK_KEYMAP_LAYER_ID_INVAL || !zmk_keymap_layer_active(layer)) {
            continue;
        }
        const struct zmk_behavior_binding *binding =
            zmk_keymap_get_layer_binding_at_idx(layer, position);
        if (binding == NULL) {
            return NULL;
        }
        if (!roamyboard_binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(trans))) ||
            layer == base) {
            return binding;
        }
    }
    return NULL;
}
