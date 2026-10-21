/*
 * Pure logic of the IQS5xx-B000 trackpad driver; iqs5xx_logic.h has the contract.
 *
 * SPDX-License-Identifier: MIT
 */

#include "iqs5xx_logic.h"

#include <string.h>

#define SWIPES (IQS5XX_SWIPE_X_NEG | IQS5XX_SWIPE_X_POS | IQS5XX_SWIPE_Y_POS | IQS5XX_SWIPE_Y_NEG)
#define MULTI_FINGER_GESTURES (IQS5XX_TWO_FINGER_TAP | IQS5XX_SCROLL | IQS5XX_ZOOM)

static int16_t be16(const uint8_t *p) { return (int16_t)(uint16_t)((p[0] << 8) | p[1]); }

void iqs5xx_frame_decode(const uint8_t raw[IQS5XX_FRAME_LEN], struct iqs5xx_frame *frame) {
    frame->gestures0 = raw[0];
    frame->gestures1 = raw[1];
    frame->system_info0 = raw[2];
    frame->system_info1 = raw[3];
    frame->fingers = raw[4];
    frame->rel_x = be16(&raw[5]);
    frame->rel_y = be16(&raw[7]);
}

void iqs5xx_logic_init(struct iqs5xx_logic *logic, int32_t scroll_divisor) {
    memset(logic, 0, sizeof(*logic));
    logic->scroll_divisor = scroll_divisor;
}

static void output_reset(struct iqs5xx_output *output) {
    output->count = 0;
    output->tap_release_due = false;
}

static void emit(struct iqs5xx_output *output, enum iqs5xx_event_kind kind, uint8_t button,
                 int32_t value, bool sync) {
    if (output->count == IQS5XX_MAX_EVENTS) {
        return; // unreachable: IQS5XX_MAX_EVENTS covers every rule firing at once
    }
    output->events[output->count++] = (struct iqs5xx_event){
        .kind = kind,
        .button = button,
        .value = value,
        .sync = sync,
    };
}

static void emit_button(struct iqs5xx_output *output, uint8_t button, bool pressed, bool sync) {
    emit(output, IQS5XX_EV_BUTTON, button, pressed ? 1 : 0, sync);
}

static void finish(struct iqs5xx_output *output) {
    if (output->count > 0) {
        output->events[output->count - 1].sync = true;
    }
}

static void tap(struct iqs5xx_logic *logic, uint8_t button, struct iqs5xx_output *output) {
    const uint8_t bit = (uint8_t)(1u << button);

    if (button == 0 && logic->holding) {
        return;
    }
    if (logic->tapped & bit) {
        // Synced, so the host sees two clicks rather than one long press.
        emit_button(output, button, false, true);
    }
    emit_button(output, button, true, false);
    logic->tapped |= bit;
    output->tap_release_due = true;
}

static int32_t wheel_steps(int32_t *remainder, int16_t rel, int32_t divisor) {
    *remainder += rel;
    const int32_t steps = *remainder / divisor;
    *remainder -= steps * divisor;
    return steps;
}

void iqs5xx_logic_frame(struct iqs5xx_logic *logic, const struct iqs5xx_frame *frame,
                        struct iqs5xx_output *output) {
    output_reset(output);

    if (frame->fingers == 0) {
        logic->scroll_x = 0;
        logic->scroll_y = 0;
        if (logic->holding) {
            logic->holding = false;
            emit_button(output, 0, false, false);
        }
    }

    if (frame->gestures0 & IQS5XX_SINGLE_TAP) {
        tap(logic, 0, output);
    }
    if (frame->gestures1 & IQS5XX_TWO_FINGER_TAP) {
        tap(logic, 1, output);
    }

    if ((frame->gestures0 & IQS5XX_PRESS_AND_HOLD) && frame->fingers > 0 && !logic->holding) {
        if (logic->tapped & 1u) {
            logic->tapped &= (uint8_t)~1u;
            emit_button(output, 0, false, true);
        }
        emit_button(output, 0, true, false);
        logic->holding = true;
    }

    if (frame->gestures1 & IQS5XX_SCROLL) {
        const int32_t hwheel = wheel_steps(&logic->scroll_x, frame->rel_x, logic->scroll_divisor);
        const int32_t wheel = wheel_steps(&logic->scroll_y, frame->rel_y, logic->scroll_divisor);
        if (hwheel) {
            emit(output, IQS5XX_EV_HWHEEL, 0, hwheel, false);
        }
        if (wheel) {
            emit(output, IQS5XX_EV_WHEEL, 0, wheel, false);
        }
    } else if (frame->fingers == 1 && !(frame->gestures0 & (IQS5XX_SINGLE_TAP | SWIPES)) &&
               !(frame->gestures1 & MULTI_FINGER_GESTURES)) {
        if (frame->rel_x) {
            emit(output, IQS5XX_EV_REL_X, 0, frame->rel_x, false);
        }
        if (frame->rel_y) {
            emit(output, IQS5XX_EV_REL_Y, 0, frame->rel_y, false);
        }
    }

    finish(output);
}

static void release_taps(struct iqs5xx_logic *logic, struct iqs5xx_output *output) {
    for (uint8_t button = 0; button < IQS5XX_BUTTONS; button++) {
        const uint8_t bit = (uint8_t)(1u << button);
        if (logic->tapped & bit) {
            logic->tapped &= (uint8_t)~bit;
            emit_button(output, button, false, false);
        }
    }
}

void iqs5xx_logic_release_taps(struct iqs5xx_logic *logic, struct iqs5xx_output *output) {
    output_reset(output);
    release_taps(logic, output);
    finish(output);
}

void iqs5xx_logic_release_all(struct iqs5xx_logic *logic, struct iqs5xx_output *output) {
    output_reset(output);
    if (logic->holding) {
        logic->holding = false;
        emit_button(output, 0, false, false);
    }
    release_taps(logic, output);
    logic->scroll_x = 0;
    logic->scroll_y = 0;
    finish(output);
}

bool iqs5xx_product_known(uint16_t product) {
    return product == 40 || product == 52 || product == 58;
}

struct iqs5xx_transition iqs5xx_next(enum iqs5xx_state state, enum iqs5xx_input input,
                                     uint16_t product) {
    const struct iqs5xx_transition keep_probing = {IQS5XX_ABSENT, IQS5XX_DO_PROBE_LATER};
    const struct iqs5xx_transition configure = {IQS5XX_CONFIGURING, IQS5XX_DO_CONFIGURE};

    switch (state) {
    case IQS5XX_ABSENT:
        if (input == IQS5XX_IN_PROBE_OK) {
            if (iqs5xx_product_known(product)) {
                return configure;
            }
            return (struct iqs5xx_transition){IQS5XX_ABSENT,
                                              IQS5XX_DO_WARN_PRODUCT | IQS5XX_DO_PROBE_LATER};
        }
        return keep_probing;

    case IQS5XX_CONFIGURING:
        switch (input) {
        case IQS5XX_IN_CONFIGURED:
            return (struct iqs5xx_transition){IQS5XX_RUNNING,
                                              IQS5XX_DO_ENABLE_RDY | IQS5XX_DO_CHECK_LATER};
        case IQS5XX_IN_PROBE_OK:
        case IQS5XX_IN_SHOW_RESET:
            return configure;
        case IQS5XX_IN_PROBE_FAILED:
        case IQS5XX_IN_IO_ERROR:
            return keep_probing;
        }
        break;

    case IQS5XX_RUNNING:
        switch (input) {
        case IQS5XX_IN_SHOW_RESET:
            return (struct iqs5xx_transition){
                IQS5XX_CONFIGURING,
                IQS5XX_DO_DISABLE_RDY | IQS5XX_DO_RELEASE | IQS5XX_DO_CONFIGURE};
        case IQS5XX_IN_PROBE_FAILED:
        case IQS5XX_IN_IO_ERROR:
            return (struct iqs5xx_transition){
                IQS5XX_ABSENT,
                IQS5XX_DO_DISABLE_RDY | IQS5XX_DO_RELEASE | IQS5XX_DO_PROBE_LATER};
        case IQS5XX_IN_PROBE_OK:
        case IQS5XX_IN_CONFIGURED:
            return (struct iqs5xx_transition){IQS5XX_RUNNING, IQS5XX_DO_CHECK_LATER};
        }
        break;
    }
    return keep_probing;
}

const char *iqs5xx_state_name(enum iqs5xx_state state) {
    switch (state) {
    case IQS5XX_ABSENT:
        return "absent";
    case IQS5XX_CONFIGURING:
        return "configuring";
    case IQS5XX_RUNNING:
        return "running";
    }
    return "unknown";
}
