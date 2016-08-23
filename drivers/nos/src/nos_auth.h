/*
 * Author: Chen Minqiang <ptpt52@gmail.com>
 *  Date : Wed, 15 Jun 2016 11:14:16 +0800
 */
#ifndef _NOS_AUTH_H_
#define _NOS_AUTH_H_
#include <linux/ctype.h>
#include <asm/types.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <net/netfilter/nf_conntrack.h>
#include <ntrack_comm.h>
#include "ntrack_auth.h"

extern unsigned int redirect_ip;
extern uint16_t g_auth_conf_magic;

struct auth_rule_t {
#define INVALID_AUTH_RULE_ID 255
	unsigned int id;
	unsigned int src_zone_id;
	unsigned int src_ipgrp_id;
#define AUTH_TYPE_UNKNOWN 0
#define AUTH_TYPE_AUTO 1
#define AUTH_TYPE_WEB 2
	unsigned int auth_type;
	unsigned int ip_white_list_id;
	unsigned int mac_white_list_id;
	struct ip_set *ip_white_list_set;
	struct ip_set *mac_white_list_set;
};

struct auth_conf {
	unsigned int num;
	unsigned int bypass_dst_list_id;
	struct ip_set *bypass_dst_list_set;
#define MAX_AUTH 16
	struct auth_rule_t auth[MAX_AUTH];
};

int nos_auth_init(void);
void nos_auth_exit(void);

unsigned int nos_auth_hook(const struct net_device *in,
		const struct net_device *out,
		struct sk_buff *skb,
		struct nf_conn *ct,
		struct nos_user_info *ui);

void nos_auth_http_302(const struct net_device *dev, struct sk_buff *skb, const struct nos_user_info *ui);

void nos_auth_convert_tcprst(struct sk_buff *skb);

#endif /* _NOS_AUTH_H_ */
