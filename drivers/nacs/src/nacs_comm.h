#ifndef _NACS_COMM_H
#define _NACS_COMM_H
#include <linux/timer.h>
#include <ntrack_nacs.h>
#include <linux/version.h>

/*fixme:just for compile error caused by ntrack_log.h*/
#define nt_debug(fmt...) do{}while(0)
#define nt_info(fmt...) do{}while(0)
#define nt_warn(fmt...) do{}while(0)
#define nt_error(fmt...) do{}while(0)
#define nt_dump(fmt...) do{}while(0)
#include <ntrack_msg.h>

struct nac_check_req {
	struct net_device *in, *out;
	struct sk_buff *skb;
	flow_info_t *fi;
	user_info_t *ui;
	user_info_t *pi;
	__u32 proto_id;
};

struct nac_table_req {
	__u8 	src_zone, dst_zone;
	__u64 	src_ipgrp_bits, dst_ipgrp_bits;
	__u32	proto_id;
};


struct dpi_flow {
	__u8 src_zone, dst_zone;
	__u64 src_ipgrp_bits, dst_ipgrp_bits;
};


static inline int ip_set_test_src_ip(const struct net_device *in, const struct net_device *out, struct sk_buff *skb, ip_set_id_t id)
{
	int ret = 0;
	struct ip_set_adt_opt opt;
	struct xt_action_param par;
	struct net *net = &init_net;
	if (in)
		net = dev_net(in);
	else if (out)
		net = dev_net(out);

	memset(&opt, 0, sizeof(opt));
	opt.family = NFPROTO_IPV4;
	opt.dim = IPSET_DIM_ONE;
	opt.flags = IPSET_DIM_ONE_SRC;
	opt.cmdflags = 0;
	opt.ext.timeout = UINT_MAX;

	par.in = in;
	par.out = out;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	par.net = net;
#endif

	ret = ip_set_test(id, skb, &par, &opt);

	return ret;
}

static inline int ip_set_test_dst_ip(const struct net_device *in, const struct net_device *out, struct sk_buff *skb, ip_set_id_t id)
{
	int ret = 0;
	struct ip_set_adt_opt opt;
	struct xt_action_param par;
	struct net *net = &init_net;
	if (in)
		net = dev_net(in);
	else if (out)
		net = dev_net(out);

	memset(&opt, 0, sizeof(opt));
	opt.family = NFPROTO_IPV4;
	opt.dim = IPSET_DIM_ONE;
	opt.flags = 0;
	opt.cmdflags = 0;
	opt.ext.timeout = UINT_MAX;

	par.in = in;
	par.out = out;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	par.net = net;
#endif

	ret = ip_set_test(id, skb, &par, &opt);

	return ret;
}

static inline int ip_set_test_src_mac(const struct net_device *in, const struct net_device *out, struct sk_buff *skb, ip_set_id_t id)
{
	int ret = 0;
	struct ip_set_adt_opt opt;
	struct xt_action_param par;
	struct net *net = &init_net;
	if (in)
		net = dev_net(in);
	else if (out)
		net = dev_net(out);

	memset(&opt, 0, sizeof(opt));
	opt.family = NFPROTO_UNSPEC;
	opt.dim = IPSET_DIM_ONE;
	opt.flags = IPSET_DIM_ONE_SRC;
	opt.cmdflags = 0;
	opt.ext.timeout = UINT_MAX;

	par.in = in;
	par.out = out;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	par.net = net;
#endif

	ret = ip_set_test(id, skb, &par, &opt);

	return ret;
}

#endif