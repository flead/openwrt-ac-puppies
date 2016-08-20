#pragma once

#include <linux/types.h>
#include <linux/list.h>

#include <linux/nos_track.h>

#include <ntrack_nproto.h>
#include <ntrack_packet.h>
#include <ntrack_flow.h>
#include <nproto/build-in.h>

#include "mwm.h"
#include "pcre.h"

#define MAX_REF_IDs 8
#define MAX_L4_ADDRS 8
#define MAX_L4_PORTS 31

#define MAX_L7_LEN_LIST 15
#define MAX_L7_LEN_RANGE 16
#define MAX_CT_MATCH_NUM 16

#define NP_PATT_LEN_MAX  256
#define NP_RULE_PRI_MIN 0
#define NP_RULE_PRI_MAX 65535
#define NP_RULES_COUNT_MAX 4096

typedef struct {
	uint32_t addrs[MAX_L4_ADDRS];
	uint16_t ports[MAX_L4_PORTS];
	uint16_t proto;
} l4_match_t;

enum __em_match_t {
	MHTP_OFFSET = 0, /* direct match l7 ptr + offset. */
	MHTP_REGEXP, /* search use regexp, advise use for this type: '/^xxx/'. */
	MHTP_SEARCH, /* search use bmh[char], use for long patt. */
	MHTP_MAX,
};

enum __em_lnm_t {
	NP_LNM_NONE = 0,
	NP_LNM_LIST,
	NP_LNM_MATCH,
	NP_LNM_RANGE,
	NP_LNM_MAX,
};

typedef struct {
	uint8_t type;  /* NONE, LIST, MATCH, RANGE, ... */
	int16_t offset; /* the curror of len info. */
	int16_t fixed; /* fixed +-n. */
	uint8_t width; /* byte, short, int -> 1,2,4 */
	uint16_t list[MAX_L7_LEN_LIST];
	uint16_t range[MAX_L7_LEN_RANGE][2];
} len_match_t;

/* rule matched callback. */
typedef int (*nproto_cb_t)(nt_packet_t *np, void *rule);
typedef int (*nproto_init_t)(void);
typedef void (*nproto_clean_t)(void);

typedef struct {
	/* 0: offset match, 1: regexp, 2: search, 3: search-offset */
	uint8_t type;

	int16_t offset;
	uint16_t deep;
	uint16_t length;
	uint8_t patt[NP_PATT_LEN_MAX];

	void *rex, *bmh;
} match_t;

typedef struct {
	uint16_t spec_len; /* only match the len == spec_len. */

	/*
	** takeoff the wrapper proto.
	**
	** 0,0: match the +OFFSET,
	** n,m: search +/- n->m
	**
	** this find the realy proto payload,
			such as http->hdr->body (http-proxy-...).
	**
	** ++++++offset[x]++***patt***+++++++++++
	** search range: offset -> offset + deep
	*/
	match_t wrap, match;
} content_match_t;

typedef struct {
	/* C->S, S->C, any */
	uint8_t dir;

	/* length info */
	len_match_t lnm;

	/* content info */
	/* or|and */
	uint8_t ctm_num:6, ctm_relation:2;
	content_match_t ctm[MAX_CT_MATCH_NUM];
} l7_match_t;

typedef struct {
	/*
	* http hdr:
	*	CTX: 0;
	*	HDR: URL, Context-type, Host, ... ;
	*/
	uint16_t hdr;
	match_t match;
} httpm_t;

typedef struct {
	/*
	* relation: OR/AND,
	*/
	uint8_t htp_relation:1;
	httpm_t htpm[MAX_CT_MATCH_NUM];
} http_match_t;

typedef struct nproto_rule np_rule_t;
/*
** rule set:
** 	UDP
**	TCP ->
		HTTP
		Not-HTTP
	Others
	REF_Rules
*/
typedef struct {
	char name[128];
	mwm_t *pmwm; /* rules with 4 char search patterns. */
	mwm_t *pmwm_host; /* for http search 'host'. */

	uint16_t num_rules;
	uint16_t capacity; /* the capacity of this set */
	np_rule_t **rules; /* the dmalloc array pointer */
} np_rule_set_t;

struct nproto_rule {
	/* rule name, app name(xunlei, web-chrome), service: http, mail, game. */
	char *name_rule, *name_app, *name_service;
	/* Rule identify crc */
	uint32_t crc;

	/* match pri, but as mwm search, this not used... */
	uint16_t priority;

	uint16_t ID;
	/* this rule ref to other/base rules. */
	uint16_t ID_REFs[MAX_REF_IDs];
	/*
	* rule_type:
	* 		base: start match as unknown, or ref to someone.
	*		ref-base: need be-refed first matched.
	*
	* refs_type:
	*		0: current-package/; 1: cross-package-in-session;
	*		2: corss-session-in-users; 4: cross-session-in-peers;
	* 		bit-map: 0/1/2/3.
	*/
	uint8_t rule_type: 4, refs_type: 4;

	/* enable the l4/l7 match process */
	uint8_t enable_l4:1, enable_l7:1, enable_http:1;

	/* l4 header match */
	l4_match_t l4;

	/* payload data match */
	l7_match_t l7;

	/* match http proto's */
	http_match_t http;

	/* ref sets */
	np_rule_set_t *ref_set;

	/* rule init/cleanup callback */
	nproto_init_t 	proto_init;
	nproto_clean_t 	proto_clean;

	/* rule match callback... */
	nproto_cb_t proto_cb;
};

enum __em_inner_sets {
	NP_SET_BASE_UDP = 0,
	NP_SET_BASE_TCP,
	NP_SET_BASE_OTHER,
	NP_SET_BASE_MAX,
};
#define SET_DIR_STR(idx) flow_dir_name[idx]
#define SET_BASE_STR(idx) set_inner_name[idx]

static inline uint8_t np_proto_to_set(uint8_t proto)
{
	switch(proto){
		case IPPROTO_TCP:
		return NP_SET_BASE_TCP;
		case IPPROTO_UDP:
		return NP_SET_BASE_UDP;
		default:
		return NP_SET_BASE_OTHER;
	}
}

enum __em_np_rule_type {
	TP_RULE_BASE = 1<<0,
	TP_RULE_MID = 1<<1,
	TP_RULE_FIN = 1<<2,
	TP_RULE_MAX,
};
#define RULE_IS_BASE(rule) 	(rule->rule_type & TP_RULE_BASE)
#define RULE_IS_MID(rule) 	(rule->rule_type & TP_RULE_MID)
#define RULE_IS_FIN(rule) 	(rule->rule_type & TP_RULE_FIN)

enum __em_np_rule_relation {
	NP_REF_NONE = 0,
	NP_REF_FLOW = 1<<0,
	NP_REF_PACKET = 1<<1,
	NP_REF_USERS = 1<<2,
};
#define RULE_REF_FLOW(rule) 	(rule->refs_type & NP_REF_FLOW)
#define RULE_REF_PACKET(rule) 	(rule->refs_type & NP_REF_PACKET)
#define RULE_REF_HTTP(rule) 	(rule->enable_http)
#define RULE_REFs(rule) (RULE_REF_FLOW(rule) || RULE_REF_PACKET(rule))

enum __em_ctm_relation {
	NP_CTM_OR = 0,
	NP_CTM_AND,
};

enum __em_result_bool {
	NP_CONTINUE = -1,
	NP_FALSE = 0,
	NP_TRUE,
};

/*
** register one rule to match system.
*/
int np_rule_register(np_rule_t *rule);

/*
** build-in inner rules init, called by modules init.
*/
int inner_rules_init(void);
