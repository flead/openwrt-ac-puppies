#define __DEBUG

#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>

#include <linux/nos_track.h>

#include "mwm.h"
#include "rules.h"

#define np_assert(x) BUG_ON(!(x))

#define NP_MWM_STR "nproto"

/* inner data struct's */
np_rule_set_t rule_sets_base[NP_FLOW_DIR_MAX][NP_SET_BASE_MAX];
np_rule_t *inner_rules[NP_INNER_RULE_MAX];

static LIST_HEAD(all_rules);

static int rule_compile(np_rule_t *rule)
{
	/* init bmh/regexp struct. */
	return 0;
}

static void rule_release(np_rule_t *rule)
{
	/* release the dmalloc mem. */
}

static void rule_dump(np_rule_t *rule)
{
	int i;

	np_print("Rule[%s] Proto[%s] Service[%s]:\n", 
		rule->name_rule, rule->name_app, rule->name_service);
	np_print("\tID: [%d]\n", rule->ID);
	for(i=0; i<MAX_REF_IDs; i++){
		if(rule->ID_REFs[i]) {
			if(i==0) {
				np_print("\tID_REFs:");
			}
			np_print(" %d", rule->ID_REFs[i]);
		} else {
			if(i)np_print("\n");
			break;
		}
	}
}

static int set_add_rule_normal_sorted(np_rule_set_t *set, np_rule_t *rule)
{
	if(rule->priority == NP_RULE_PRI_MIN) {
		set->rules[set->num_rules] = rule;
	} else if(rule->priority == NP_RULE_PRI_MAX) {
		if(set->num_rules > 0)
			memmove(&set->rules[1], set->rules, sizeof(set->rules[0]) * set->num_rules);
		set->rules[0] = rule;
	} else {
		/* find the insert position */
		int L = 0;
		int H = set->num_rules - 1;
		int M, C;
		while(L <= H) {
			M = (L + H) / 2;
			if(set->rules[M]->priority == rule->priority) {
				H = M;
				break;
			} else {
				if(set->rules[M]->priority < rule->priority) {
					H = M - 1; /* < */
				} else {
					L = M + 1; /* > */
				}
			}
		}
		/* move [H],([H+1]),[H+2]... */
		C = set->num_rules - (H + 1);
		if(C > 0) {
			memmove(&set->rules[H + 2], &set->rules[H + 1], sizeof(set->rules[0]) * C);
		}
		set->rules[H + 1] = rule;
	}
	set->num_rules ++;
	return 0;
}

#define NP_SET_GROW_COUNT 64
static int set_add_rule_normal(np_rule_set_t *set, np_rule_t *rule)
{
	if(set->num_rules >= set->capacity) {
		uint32_t nsize = (set->capacity + NP_SET_GROW_COUNT) * sizeof(set->rules[0]);
		np_rule_t **po = set->rules;
		np_rule_t **pn = (np_rule_t **)vmalloc(nsize);
		if (!pn) {
			np_error("re-alloc mem failed: %d\n", nsize);
			return -ENOMEM;
		}
		memcpy(pn, po, set->capacity * sizeof(set->rules[0]));
		set->rules = pn;
		set->capacity += NP_SET_GROW_COUNT;
		vfree(po);
	}

	return set_add_rule_normal_sorted(set, rule);
}

static int set_add_rule(np_rule_set_t *set, np_rule_t *rule)
{
	int i, n, rule_in_mwm = 0;

	np_assert(set);
	np_assert(rule);

	/* check l7 search && patt len > 4 */
	if(!rule->enable_l7) {
		set_add_rule_normal(set, rule);
	}

	for(i=0; i<rule->l7.ctm_num; i++) {
		mwm_t *mwm = set->pmwm;
		content_match_t *ct = &rule->l7.ctm[i];

		/* search & patt length >= 4 */
		if(!(ct->type == MHTP_SEARCH && ct->patt_len >= 4))
			continue;

		/* init mwm st.. */
		if(!mwm) {
			mwm = mwmNew();
			if(mwm) {
				np_error("create mwm failed.\n");
				return set_add_rule_normal(set, rule);
			}
			set->pmwm = mwm;
		}
		/* add patters & rule to mwm. */
		n = mwmAddPatternEx(mwm, ct->patt, ct->patt_len, ct->offset, ct->deep, rule);
		if(n<0) {
			np_error("mwm prepare %s:%d error.\n", rule->name_rule, rule->ID);
			return set_add_rule_normal(set, rule);
		} else {
			rule_in_mwm = 1;
			np_info("mwm add rule[%s:%d] ct[%d]\n", rule->name_rule, rule->ID, i);
		}
	}

	/* default add to normal set */
	if(!rule_in_mwm) {
		return set_add_rule_normal(set, rule);
	}

	return 0;
}

static int np_rule_register(np_rule_t *rule)
{
	int dir = 0, proto = NP_SET_BASE_OTHER;
	/* compile && check valid */
	if(rule_compile(rule)){
		np_error("compile %d: %s\n", rule->ID, rule->name_rule);
		return -EINVAL;
	}

	/* add to global list. */
	list_add_tail(&rule->list, &all_rules);

	if(!rule->base_rule) {
		return 0;
	}

	/* base rule, to inner set's */
	if (rule->enable_l4) {
		proto = np_proto_to_set(rule->l4.proto);
	}

	if(rule->enable_l7) {
		dir = rule->l7.dir;
	}

	/* invalid rule pars. */
	np_assert(dir < NP_FLOW_DIR_MAX);
	np_assert(proto < NP_SET_BASE_MAX);

	np_info("base rule: %s, id: %d\n", rule->name_rule, rule->ID);
	return set_add_rule(&rule_sets_base[dir][proto], rule);
}

static void set_clean(np_rule_set_t *set)
{
	if(set->pmwm) {
		mwmFree(set->pmwm);
	}
}

static int set_init(np_rule_set_t *set, char *name)
{
	int ret;

	snprintf(set->name, sizeof(set->name), "%s", name);
	if(set->pmwm) {
		ret = mwmPrepPatterns(set->pmwm);
		if(ret < 0) {
			np_error("init mwm - failed.\n");
			return -EINVAL;
		} else if(ret == 0) {
			mwmFree(set->pmwm);
			set->pmwm = NULL;
		} else {
			mwmGroupDetails(set->pmwm);
		}
	}
	return 0;
}

static void set_dump(np_rule_set_t *set)
{
	int i;

	if(!set->num_rules && !set->pmwm) {
		/* empty */
		return;
	}

	np_print(" ---- rule-set [%s] ---- \n", set->name);
	if(set->pmwm) {
		mwmGroupDetails(set->pmwm);
	}
	for(i=0; i<set->num_rules; i++) {
		np_rule_t *rule = set->rules[i];
		rule_dump(rule);
	}
	return;
}

static void rules_cleanup(void)
{
	int i, j;
	struct list_head *itr;

	/* clean rule set's */
	for(i=0; i<NP_FLOW_DIR_MAX; i++) {
		for(j=0; j<NP_SET_BASE_MAX; j++){
			set_clean(&rule_sets_base[i][j]);
		}
	}
	list_for_each(itr, &all_rules) {
		np_rule_t *rule = list_entry(itr, np_rule_t, list);
	
		/* cleanup refs. */
		rule_release(rule);
		set_clean(&rule->ref_set);
	}
}

static int rules_build(void)
{
	int i, j;
	char name[64];
	struct list_head *itr1, *itr2;

	/* build each ref's */
	list_for_each(itr1, &all_rules) {
		np_rule_t *rule1 = list_entry(itr1, np_rule_t, list);
		/* each ref id find the target rule. */
		for(i=0; i<sizeof(rule1->ID_REFs)/sizeof(rule1->ID_REFs[0]); i++) {
			uint16_t ref_id = rule1->ID_REFs[i];
			if(!ref_id) {
				break;
			}
			/* round2 find the ref target. */
			list_for_each(itr2, &all_rules) {
				np_rule_t *rule2 = list_entry(itr2, np_rule_t, list);
				if(rule1 == rule2) { /* self */
					continue;
				}
				if(rule2->ID == ref_id) {
					/* got it */
					set_add_rule(&rule1->ref_set, rule2);
				}
			}
		}
	}

	/* init all set's */
	for(i=0; i<NP_FLOW_DIR_MAX; i++) {
		for(j=0; j<NP_SET_BASE_MAX; j++) {
			snprintf(name, sizeof(name), "base: %s,%d", i?"c2s":"s2c", j);
			set_init(&rule_sets_base[i][j], name);
		}
	}
	list_for_each(itr1, &all_rules) {
		np_rule_t *rule = list_entry(itr1, np_rule_t, list);
		snprintf(name, sizeof(name), "ref: %s", rule->name_rule);
		set_init(&rule->ref_set, name);
	}

	#if 1
	/* dump rules */
	for(i=0; i<NP_FLOW_DIR_MAX; i++) {
		for(j=0; j<NP_SET_BASE_MAX; j++)
		 set_dump(&rule_sets_base[i][j]);
	}
	#endif

	return 0;
}

static int inner_rules_init(void)
{
	extern np_rule_t inner_http_req, inner_http_rep;

	np_rule_register(&inner_http_req);
	np_rule_register(&inner_http_rep);

	return 0;
}

static int rule_matched(void *nt, void *rule)
{
	return 1;
}

static int rule_one_match(np_rule_t *rule, 
	struct nos_track *nt, 
	struct sk_buff *skb, 
	uint8_t *data, int dlen, 
	int(*matched_cb)(void *nt, void *rule))
{
	/* do match process. */


	/* rule all matched. */
	if(rule->proto_cb) {
		rule->proto_cb(nt, skb, rule);
	}
	if(matched_cb) {
		return matched_cb(nt, rule);
	}
	return 1;
}

/* mwm transmit par in-to sub-function's... */
typedef struct {
	struct nos_track *nt;
	struct sk_buff *skb;
	uint8_t *data;
	int dlen;
} mwmIn_t;

/*
** @return: !=0 -> ture, 0:false -> try next.
*/
static int mwmOnMatch(void* rule, void *inv, void *out)
{
	mwmIn_t *in = (mwmIn_t*)inv;

	if(rule_one_match(rule, in->nt, in->skb, in->data, in->dlen, rule_matched)) {
		out = rule;
		return 1;
	}
	return 0;
}

int rules_match(struct nos_track* nt,
	struct sk_buff *skb, int fdir, uint8_t proto, 
	uint8_t *data, int dlen)
{
	int i;
	np_rule_set_t *base_set = &rule_sets_base[fdir][np_proto_to_set(proto)];
	mwm_t *pmwm = base_set->pmwm;

	for (i = 0; i <base_set->num_rules; i++) {
		np_rule_t *rule = base_set->rules[i];
		if(rule_one_match(rule, nt, skb, data, dlen, rule_matched)) {
			/* direct matched. */
			np_debug("direct matched: %s\n", rule->name_rule);
			return 1;
		}
	}

	if(pmwm) {
		void* out = NULL;
		mwmIn_t in;

		in.nt = nt;
		in.skb = skb;
		in.data = data;
		in.dlen = dlen;
		mwmSearch(pmwm, data, dlen, &in, &out, mwmOnMatch);
		if(out) {
			np_rule_t *o = out;
			np_debug("mwm matched: %s\n", o->name_rule);
			return 1;
		}
	}

	return 0;
}

int nproto_init(void)
{
	int ret;

	mwmSysInit(NP_MWM_STR);

	memset(&inner_rules, 0, sizeof(inner_rules));
	memset(&rule_sets_base, 0, sizeof(rule_sets_base));

	ret = inner_rules_init();
	if(ret) {
		np_error("inner rules init failed.\n");
		return ret;
	}

	ret = rules_build();
	if(ret){
		np_error("rules build.\n");
		return ret;
	}
	return 0;
}

void nproto_cleanup(void)
{
	rules_cleanup();
	mwmSysClean(NP_MWM_STR);
}