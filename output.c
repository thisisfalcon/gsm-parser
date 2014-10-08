#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <osmocom/gsm/rsl.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>

#include "output.h"

static struct gsmtap_inst *gti = 0;

void net_init()
{
	gti = gsmtap_source_init("127.0.0.1", GSMTAP_UDP_PORT, 0);
	gsmtap_source_add_sink(gti);
}

void net_send_rlcmac(uint8_t *msg, int len, uint8_t ul)
{
	if (gti) {
		gsmtap_send(gti, ul?ARFCN_UPLINK:0, 0, 0xd, 0, 0, 0, 0, msg, len);
	}
}

void net_send_llc(uint8_t *data, int len, uint8_t ul)
{
	struct msgb *msg;
	struct gsmtap_hdr *gh;
	uint8_t *dst;

	if (!gti)
		return;

	if ((data[0] == 0x43) &&
	    (data[1] == 0xc0) &&
	    (data[2] == 0x01))
		return;

	msg = msgb_alloc(sizeof(*gh) + len, "gsmtap_tx");
	if (!msg)
	        return;

	gh = (struct gsmtap_hdr *) msgb_put(msg, sizeof(*gh));

	gh->version = GSMTAP_VERSION;
	gh->hdr_len = sizeof(*gh)/4;
	gh->type = 8;
	gh->timeslot = 0;
	gh->sub_slot = 0;
	gh->arfcn = ul ? htons(ARFCN_UPLINK) : 0;
	gh->snr_db = 0;
	gh->signal_dbm = 0;
	gh->frame_number = 0;
	gh->sub_type = 0;
	gh->antenna_nr = 0;

        dst = msgb_put(msg, len);
        memcpy(dst, data, len);

	gsmtap_sendmsg(gti, msg);
}

void net_send_msg(struct radio_message *m)
{
	uint8_t ts, type, subch;
	struct msgb *msgb = 0;

	if (!(gti && (m->flags & MSG_DECODED)))
		return;

	switch (m->rat) {
	case RAT_GSM:
		rsl_dec_chan_nr(m->chan_nr, &type, &subch, &ts);
		msgb = gsmtap_makemsg(m->bb.arfcn[0], ts,
				      chantype_rsl2gsmtap(type, (m->flags & MSG_SACCH) ? 0x40 : 0),
				      subch, m->bb.fn[0], m->bb.rxl[0], m->bb.snr[0], m->msg, m->msg_len);
		break;
	case RAT_UMTS:
		msgb = gsmtap_makemsg_ex(GSMTAP_TYPE_UMTS_RRC, m->bb.arfcn[0], 0,
					 m->bb.arfcn[0] & ARFCN_UPLINK ? GSMTAP_RRC_SUB_UL_DCCH_Message : GSMTAP_RRC_SUB_DL_DCCH_Message,
					 0, 0, 0, 0, m->msg, m->msg_len);
		break;
	}

	if (msgb)
		gsmtap_sendmsg(gti, msgb);
}
