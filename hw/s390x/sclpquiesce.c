/*
 * SCLP event type
 *    Signal Quiesce - trigger system powerdown request
 *
 * Copyright IBM, Corp. 2012
 *
 * Authors:
 *  Heinz Graalfs <graalfs@de.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at your
 * option) any later version.  See the COPYING file in the top-level directory.
 *
 */
#include <hw/qdev.h>
#include "sysemu/sysemu.h"
#include "hw/s390x/sclp.h"
#include "hw/s390x/event-facility.h"

typedef struct SignalQuiesce {
    EventBufferHeader ebh;
    uint16_t timeout;
    uint8_t unit;
} QEMU_PACKED SignalQuiesce;

static int event_type(void)
{
    return SCLP_EVENT_SIGNAL_QUIESCE;
}

static unsigned int send_mask(void)
{
    return SCLP_EVENT_MASK_SIGNAL_QUIESCE;
}

static unsigned int receive_mask(void)
{
    return 0;
}

static int read_event_data(SCLPEvent *event, EventBufferHeader *evt_buf_hdr,
                           int *slen)
{
    SignalQuiesce *sq = (SignalQuiesce *) evt_buf_hdr;

    if (*slen < sizeof(SignalQuiesce)) {
        return 0;
    }

    if (!event->event_pending) {
        return 0;
    }
    event->event_pending = false;

    sq->ebh.length = cpu_to_be16(sizeof(SignalQuiesce));
    sq->ebh.type = SCLP_EVENT_SIGNAL_QUIESCE;
    sq->ebh.flags |= SCLP_EVENT_BUFFER_ACCEPTED;
    /*
     * system_powerdown does not have a timeout. Fortunately the
     * timeout value is currently ignored by Linux, anyway
     */
    sq->timeout = cpu_to_be16(0);
    sq->unit = cpu_to_be16(0);
    *slen -= sizeof(SignalQuiesce);

    return 1;
}

typedef struct QuiesceNotifier QuiesceNotifier;

static struct QuiesceNotifier {
    Notifier notifier;
    SCLPEvent *event;
} qn;

static void quiesce_powerdown_req(Notifier *n, void *opaque)
{
    QuiesceNotifier *qn = container_of(n, QuiesceNotifier, notifier);
    SCLPEvent *event = qn->event;

    event->event_pending = true;
    /* trigger SCLP read operation */
    sclp_service_interrupt(0);
}

static int quiesce_init(SCLPEvent *event)
{
    event->event_type = SCLP_EVENT_SIGNAL_QUIESCE;

    qn.notifier.notify = quiesce_powerdown_req;
    qn.event = event;

    qemu_register_powerdown_notifier(&qn.notifier);

    return 0;
}

static void quiesce_class_init(ObjectClass *klass, void *data)
{
    SCLPEventClass *k = SCLP_EVENT_CLASS(klass);

    k->init = quiesce_init;

    k->get_send_mask = send_mask;
    k->get_receive_mask = receive_mask;
    k->event_type = event_type;
    k->read_event_data = read_event_data;
    k->write_event_data = NULL;
}

static const TypeInfo sclp_quiesce_info = {
    .name          = "sclpquiesce",
    .parent        = TYPE_SCLP_EVENT,
    .instance_size = sizeof(SCLPEvent),
    .class_init    = quiesce_class_init,
    .class_size    = sizeof(SCLPEventClass),
};

static void register_types(void)
{
    type_register_static(&sclp_quiesce_info);
}

type_init(register_types)
