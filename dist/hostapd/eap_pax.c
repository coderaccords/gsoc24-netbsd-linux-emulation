/*
 * hostapd / EAP-PAX (draft-clancy-eap-pax-04.txt) server
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "eap_pax_common.h"

/*
 * Note: only PAX_STD subprotocol is currently supported
 */

struct eap_pax_data {
	enum { PAX_STD_1, PAX_STD_3, SUCCESS, FAILURE } state;
	u8 mac_id;
	union {
		u8 e[2 * EAP_PAX_RAND_LEN];
		struct {
			u8 x[EAP_PAX_RAND_LEN]; /* server rand */
			u8 y[EAP_PAX_RAND_LEN]; /* client rand */
		} r;
	} rand;
	u8 ak[EAP_PAX_AK_LEN];
	u8 mk[EAP_PAX_MK_LEN];
	u8 ck[EAP_PAX_CK_LEN];
	u8 ick[EAP_PAX_ICK_LEN];
	int keys_set;
	char *cid;
	size_t cid_len;
};


static void * eap_pax_init(struct eap_sm *sm)
{
	struct eap_pax_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->state = PAX_STD_1;
	/*
	 * TODO: make this configurable once EAP_PAX_MAC_AES_CBC_MAC_128 is
	 * supported
	 */
	data->mac_id = EAP_PAX_MAC_HMAC_SHA1_128;

	return data;
}


static void eap_pax_reset(struct eap_sm *sm, void *priv)
{
	struct eap_pax_data *data = priv;
	free(data->cid);
	free(data);
}


static u8 * eap_pax_build_std_1(struct eap_sm *sm,
				struct eap_pax_data *data,
				int id, size_t *reqDataLen)
{
	struct eap_pax_hdr *req;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX_STD-1 (sending)");

	if (hostapd_get_rand(data->rand.r.x, EAP_PAX_RAND_LEN)) {
		wpa_printf(MSG_ERROR, "EAP-PAX: Failed to get random data");
		data->state = FAILURE;
		return NULL;
	}

	*reqDataLen = sizeof(*req) + 2 + EAP_PAX_RAND_LEN + EAP_PAX_ICV_LEN;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PAX: Failed to allocate memory "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_PAX;
	req->op_code = EAP_PAX_OP_STD_1;
	req->flags = 0;
	req->mac_id = data->mac_id;
	req->dh_group_id = EAP_PAX_DH_GROUP_NONE;
	req->public_key_id = EAP_PAX_PUBLIC_KEY_NONE;
	pos = (u8 *) (req + 1);
	*pos++ = 0;
	*pos++ = EAP_PAX_RAND_LEN;
	memcpy(pos, data->rand.r.x, EAP_PAX_RAND_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: A = X (server rand)",
		    pos, EAP_PAX_RAND_LEN);
	pos += EAP_PAX_RAND_LEN;

	eap_pax_mac(data->mac_id, (u8 *) "", 0,
		    (u8 *) req, *reqDataLen - EAP_PAX_ICV_LEN,
		    NULL, 0, NULL, 0, pos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", pos, EAP_PAX_ICV_LEN);
	pos += EAP_PAX_ICV_LEN;

	return (u8 *) req;
}


static u8 * eap_pax_build_std_3(struct eap_sm *sm,
				struct eap_pax_data *data,
				int id, size_t *reqDataLen)
{
	struct eap_pax_hdr *req;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "EAP-PAX: PAX_STD-3 (sending)");

	*reqDataLen = sizeof(*req) + 2 + EAP_PAX_MAC_LEN + EAP_PAX_ICV_LEN;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-PAX: Failed to allocate memory "
			   "request");
		data->state = FAILURE;
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	req->type = EAP_TYPE_PAX;
	req->op_code = EAP_PAX_OP_STD_3;
	req->flags = 0;
	req->mac_id = data->mac_id;
	req->dh_group_id = EAP_PAX_DH_GROUP_NONE;
	req->public_key_id = EAP_PAX_PUBLIC_KEY_NONE;
	pos = (u8 *) (req + 1);
	*pos++ = 0;
	*pos++ = EAP_PAX_MAC_LEN;
	eap_pax_mac(data->mac_id, data->ck, EAP_PAX_CK_LEN,
		    data->rand.r.y, EAP_PAX_RAND_LEN,
		    (u8 *) data->cid, data->cid_len, NULL, 0, pos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: MAC_CK(B, CID)",
		    pos, EAP_PAX_MAC_LEN);
	pos += EAP_PAX_MAC_LEN;

	eap_pax_mac(data->mac_id, data->ick, EAP_PAX_ICK_LEN,
		    (u8 *) req, *reqDataLen - EAP_PAX_ICV_LEN,
		    NULL, 0, NULL, 0, pos);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", pos, EAP_PAX_ICV_LEN);
	pos += EAP_PAX_ICV_LEN;

	return (u8 *) req;
}


static u8 * eap_pax_buildReq(struct eap_sm *sm, void *priv, int id,
				  size_t *reqDataLen)
{
	struct eap_pax_data *data = priv;

	switch (data->state) {
	case PAX_STD_1:
		return eap_pax_build_std_1(sm, data, id, reqDataLen);
	case PAX_STD_3:
		return eap_pax_build_std_3(sm, data, id, reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-PAX: Unknown state %d in buildReq",
			   data->state);
		break;
	}
	return NULL;
}


static Boolean eap_pax_check(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_pax_data *data = priv;
	struct eap_pax_hdr *resp;
	size_t len;
	u8 icvbuf[EAP_PAX_ICV_LEN], *icv;

	resp = (struct eap_pax_hdr *) respData;
	if (respDataLen < sizeof(*resp) || resp->type != EAP_TYPE_PAX ||
	    (len = ntohs(resp->length)) > respDataLen ||
	    len < sizeof(*resp) + EAP_PAX_ICV_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: Invalid frame");
		return TRUE;
	}

	wpa_printf(MSG_DEBUG, "EAP-PAX: received frame: op_code 0x%x "
		   "flags 0x%x mac_id 0x%x dh_group_id 0x%x "
		   "public_key_id 0x%x",
		   resp->op_code, resp->flags, resp->mac_id, resp->dh_group_id,
		   resp->public_key_id);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: received payload",
		    (u8 *) (resp + 1), len - sizeof(*resp) - EAP_PAX_ICV_LEN);

	if (data->state == PAX_STD_1 &&
	    resp->op_code != EAP_PAX_OP_STD_2) {
		wpa_printf(MSG_DEBUG, "EAP-PAX: Expected PAX_STD-2 - "
			   "ignore op %d", resp->op_code);
		return TRUE;
	}

	if (data->state == PAX_STD_3 &&
	    resp->op_code != EAP_PAX_OP_ACK) {
		wpa_printf(MSG_DEBUG, "EAP-PAX: Expected PAX-ACK - "
			   "ignore op %d", resp->op_code);
		return TRUE;
	}

	if (resp->op_code != EAP_PAX_OP_STD_2 &&
	    resp->op_code != EAP_PAX_OP_ACK) {
		wpa_printf(MSG_DEBUG, "EAP-PAX: Unknown op_code 0x%x",
			   resp->op_code);
	}

	if (data->mac_id != resp->mac_id) {
		wpa_printf(MSG_DEBUG, "EAP-PAX: Expected MAC ID 0x%x, "
			   "received 0x%x", data->mac_id, resp->mac_id);
		return TRUE;
	}

	if (resp->dh_group_id != EAP_PAX_DH_GROUP_NONE) {
		wpa_printf(MSG_INFO, "EAP-PAX: Expected DH Group ID 0x%x, "
			   "received 0x%x", EAP_PAX_DH_GROUP_NONE,
			   resp->dh_group_id);
		return TRUE;
	}

	if (resp->public_key_id != EAP_PAX_PUBLIC_KEY_NONE) {
		wpa_printf(MSG_INFO, "EAP-PAX: Expected Public Key ID 0x%x, "
			   "received 0x%x", EAP_PAX_PUBLIC_KEY_NONE,
			   resp->public_key_id);
		return TRUE;
	}

	if (resp->flags & EAP_PAX_FLAGS_MF) {
		/* TODO: add support for reassembling fragments */
		wpa_printf(MSG_INFO, "EAP-PAX: fragmentation not supported");
		return TRUE;
	}

	if (resp->flags & EAP_PAX_FLAGS_CE) {
		wpa_printf(MSG_INFO, "EAP-PAX: Unexpected CE flag");
		return TRUE;
	}

	if (data->keys_set) {
		if (len - sizeof(*resp) < EAP_PAX_ICV_LEN) {
			wpa_printf(MSG_INFO, "EAP-PAX: No ICV in the packet");
			return TRUE;
		}
		icv = respData + len - EAP_PAX_ICV_LEN;
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", icv, EAP_PAX_ICV_LEN);
		eap_pax_mac(data->mac_id, data->ick, EAP_PAX_ICK_LEN,
			    respData, len - EAP_PAX_ICV_LEN, NULL, 0, NULL, 0,
			    icvbuf);
		if (memcmp(icvbuf, icv, EAP_PAX_ICV_LEN) != 0) {
			wpa_printf(MSG_INFO, "EAP-PAX: Invalid ICV");
			wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: Expected ICV",
				    icvbuf, EAP_PAX_ICV_LEN);
			return TRUE;
		}
	}

	return FALSE;
}


static void eap_pax_process_std_2(struct eap_sm *sm,
				  struct eap_pax_data *data,
				  u8 *respData, size_t respDataLen)
{
	struct eap_pax_hdr *resp;
	u8 *pos, mac[EAP_PAX_MAC_LEN], icvbuf[EAP_PAX_ICV_LEN];
	size_t len, left;
	int i;

	if (data->state != PAX_STD_1)
		return;

	wpa_printf(MSG_DEBUG, "EAP-PAX: Received PAX_STD-2");

	resp = (struct eap_pax_hdr *) respData;
	len = ntohs(resp->length);
	pos = (u8 *) (resp + 1);
	left = len - sizeof(*resp);

	if (left < 2 + EAP_PAX_RAND_LEN ||
	    ((pos[0] << 8) | pos[1]) != EAP_PAX_RAND_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: Too short PAX_STD-2 (B)");
		return;
	}
	pos += 2;
	left -= 2;
	memcpy(data->rand.r.y, pos, EAP_PAX_RAND_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: Y (client rand)",
		    data->rand.r.y, EAP_PAX_RAND_LEN);
	pos += EAP_PAX_RAND_LEN;
	left -= EAP_PAX_RAND_LEN;

	if (left < 2 || 2 + ((pos[0] << 8) | pos[1]) > left) {
		wpa_printf(MSG_INFO, "EAP-PAX: Too short PAX_STD-2 (CID)");
		return;
	}
	data->cid_len = (pos[0] << 8) | pos[1];
	free(data->cid);
	data->cid = malloc(data->cid_len);
	if (data->cid == NULL) {
		wpa_printf(MSG_INFO, "EAP-PAX: Failed to allocate memory for "
			   "CID");
		return;
	}
	memcpy (data->cid, pos + 2, data->cid_len);
	pos += 2 + data->cid_len;
	left -= 2 + data->cid_len;
	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-PAX: CID",
			  (u8 *) data->cid, data->cid_len);

	if (left < 2 + EAP_PAX_MAC_LEN ||
	    ((pos[0] << 8) | pos[1]) != EAP_PAX_MAC_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: Too short PAX_STD-2 (MAC_CK)");
		return;
	}
	pos += 2;
	left -= 2;
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: MAC_CK(A, B, CID)",
		    pos, EAP_PAX_MAC_LEN);

	if (eap_user_get(sm, (u8 *) data->cid, data->cid_len, 0) < 0) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-PAX: unknown CID",
				  (u8 *) data->cid, data->cid_len);
		data->state = FAILURE;
		return;
	}

	for (i = 0;
	     i < EAP_MAX_METHODS && sm->user->methods[i] != EAP_TYPE_NONE;
	     i++) {
		if (sm->user->methods[i] == EAP_TYPE_PAX)
			break;
	}

	if (sm->user->methods[i] != EAP_TYPE_PAX) {
		wpa_hexdump_ascii(MSG_DEBUG,
				  "EAP-PAX: EAP-PAX not enabled for CID",
				  (u8 *) data->cid, data->cid_len);
		data->state = FAILURE;
		return;
	}

	if (sm->user->password == NULL ||
	    sm->user->password_len != EAP_PAX_AK_LEN) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-PAX: invalid password in "
				  "user database for CID",
				  (u8 *) data->cid, data->cid_len);
		data->state = FAILURE;
		return;
	}
	memcpy(data->ak, sm->user->password, EAP_PAX_AK_LEN);

	if (eap_pax_initial_key_derivation(data->mac_id, data->ak,
					   data->rand.e, data->mk, data->ck,
					   data->ick) < 0) {
		wpa_printf(MSG_INFO, "EAP-PAX: Failed to complete initial "
			   "key derivation");
		data->state = FAILURE;
		return;
	}
	data->keys_set = 1;

	eap_pax_mac(data->mac_id, data->ck, EAP_PAX_CK_LEN,
		    data->rand.r.x, EAP_PAX_RAND_LEN,
		    data->rand.r.y, EAP_PAX_RAND_LEN,
		    (u8 *) data->cid, data->cid_len, mac);
	if (memcmp(mac, pos, EAP_PAX_MAC_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-PAX: Invalid MAC_CK(A, B, CID) in "
			   "PAX_STD-2");
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: Expected MAC_CK(A, B, CID)",
			    mac, EAP_PAX_MAC_LEN);
		data->state = FAILURE;
		return;
	}

	pos += EAP_PAX_MAC_LEN;
	left -= EAP_PAX_MAC_LEN;

	if (left < EAP_PAX_ICV_LEN) {
		wpa_printf(MSG_INFO, "EAP-PAX: Too short ICV (%lu) in "
			   "PAX_STD-2", (unsigned long) left);
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ICV", pos, EAP_PAX_ICV_LEN);
	eap_pax_mac(data->mac_id, data->ick, EAP_PAX_ICK_LEN,
		    respData, len - EAP_PAX_ICV_LEN, NULL, 0, NULL, 0, icvbuf);
	if (memcmp(icvbuf, pos, EAP_PAX_ICV_LEN) != 0) {
		wpa_printf(MSG_INFO, "EAP-PAX: Invalid ICV in PAX_STD-2");
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: Expected ICV",
			    icvbuf, EAP_PAX_ICV_LEN);
		return;
	}
	pos += EAP_PAX_ICV_LEN;
	left -= EAP_PAX_ICV_LEN;

	if (left > 0) {
		wpa_hexdump(MSG_MSGDUMP, "EAP-PAX: ignored extra payload",
			    pos, left);
	}

	data->state = PAX_STD_3;
}


static void eap_pax_process_ack(struct eap_sm *sm,
				struct eap_pax_data *data,
				u8 *respData, size_t respDataLen)
{
	if (data->state != PAX_STD_3)
		return;

	wpa_printf(MSG_DEBUG, "EAP-PAX: Received PAX-ACK - authentication "
		   "completed successfully");
	data->state = SUCCESS;
}


static void eap_pax_process(struct eap_sm *sm, void *priv,
				 u8 *respData, size_t respDataLen)
{
	struct eap_pax_data *data = priv;
	struct eap_pax_hdr *resp;

	if (sm->user == NULL || sm->user->password == NULL) {
		wpa_printf(MSG_INFO, "EAP-PAX: Password not configured");
		data->state = FAILURE;
		return;
	}

	resp = (struct eap_pax_hdr *) respData;

	switch (resp->op_code) {
	case EAP_PAX_OP_STD_2:
		eap_pax_process_std_2(sm, data, respData, respDataLen);
		break;
	case EAP_PAX_OP_ACK:
		eap_pax_process_ack(sm, data, respData, respDataLen);
		break;
	}
}


static Boolean eap_pax_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_pax_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_pax_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_pax_data *data = priv;
	u8 *key;

	if (data->state != SUCCESS)
		return NULL;

	key = malloc(EAP_PAX_MSK_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_PAX_MSK_LEN;
	eap_pax_kdf(data->mac_id, data->mk, EAP_PAX_MK_LEN,
		    "Master Session Key", data->rand.e, 2 * EAP_PAX_RAND_LEN,
		    EAP_PAX_MSK_LEN, key);

	return key;
}


static Boolean eap_pax_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_pax_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_pax =
{
	.method = EAP_TYPE_PAX,
	.name = "PAX",
	.init = eap_pax_init,
	.reset = eap_pax_reset,
	.buildReq = eap_pax_buildReq,
	.check = eap_pax_check,
	.process = eap_pax_process,
	.isDone = eap_pax_isDone,
	.getKey = eap_pax_getKey,
	.isSuccess = eap_pax_isSuccess,
};
