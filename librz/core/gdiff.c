/* radare - LGPL - Copyright 2010-2019 - nibble, pancake */

#include <stdio.h>
#include <string.h>
#include <rz_anal.h>
#include <rz_list.h>
#include <rz_util.h>
#include <rz_core.h>

RZ_API int rz_core_gdiff_fcn(RzCore *c, ut64 addr, ut64 addr2) {
	RzList *la, *lb;
	RzAnalFunction *fa = rz_anal_get_function_at (c->anal, addr);
	RzAnalFunction *fb = rz_anal_get_function_at (c->anal, addr2);
	if (!fa || !fb) {
		return false;
	}
	RzAnalBlock *bb;
	RzListIter *iter;
	rz_list_foreach (fa->bbs, iter, bb) {
		rz_anal_diff_fingerprint_bb (c->anal, bb);
	}
	rz_list_foreach (fb->bbs, iter, bb) {
		rz_anal_diff_fingerprint_bb (c->anal, bb);
	}
	la = rz_list_new ();
	rz_list_append (la, fa);
	lb = rz_list_new ();
	rz_list_append (lb, fb);
	rz_anal_diff_fcn (c->anal, la, lb);
	rz_list_free (la);
	rz_list_free (lb);
	return true;
}

/* Fingerprint functions and blocks, then diff. */
RZ_API int rz_core_gdiff(RzCore *c, RzCore *c2) {
	RzCore *cores[2] = {c, c2};
	RzAnalFunction *fcn;
	RzAnalBlock *bb;
	RzListIter *iter, *iter2;
	int i;

	if (!c || !c2) {
		return false;
	}
	for (i = 0; i < 2; i++) {
		/* remove strings */
		rz_list_foreach_safe (cores[i]->anal->fcns, iter, iter2, fcn) {
			if (!strncmp (fcn->name, "str.", 4)) {
				rz_anal_function_delete (fcn);
			}
		}
		/* Fingerprint fcn bbs (functions basic-blocks) */
		rz_list_foreach (cores[i]->anal->fcns, iter, fcn) {
			rz_list_foreach (fcn->bbs, iter2, bb) {
				rz_anal_diff_fingerprint_bb (cores[i]->anal, bb);
			}
		}
		/* Fingerprint fcn */
		rz_list_foreach (cores[i]->anal->fcns, iter, fcn) {
			rz_anal_diff_fingerprint_fcn (cores[i]->anal, fcn);
		}
	}
	/* Diff functions */
	rz_anal_diff_fcn (cores[0]->anal, cores[0]->anal->fcns, cores[1]->anal->fcns);

	return true;
}

/* copypasta from rz_diff */
static void diffrow(ut64 addr, const char *name, ut32 size, int maxnamelen,
		int digits, ut64 addr2, const char *name2, ut32 size2,
		const char *match, double dist, int bare) {
	if (bare) {
		if (addr2 == UT64_MAX || !name2) {
			printf ("0x%016"PFMT64x" |%8s  (%f)\n", addr, match, dist);
		} else {
			printf ("0x%016"PFMT64x" |%8s  (%f) | 0x%016"PFMT64x"\n", addr, match, dist, addr2);
		}
	} else {
		if (addr2 == UT64_MAX || !name2) {
			printf ("%*s %*d 0x%"PFMT64x" |%8s  (%f)\n",
				maxnamelen, name, digits, size, addr, match, dist);
		} else {
			printf ("%*s %*d 0x%"PFMT64x" |%8s  (%f) | 0x%"PFMT64x"  %*d %s\n",
				maxnamelen, name, digits, size, addr, match, dist, addr2,
				digits, size2, name2);
		}
	}
}

RZ_API void rz_core_diff_show(RzCore *c, RzCore *c2) {
        bool bare = rz_config_get_i (c->config, "diff.bare") || rz_config_get_i (c2->config, "diff.bare");
        RzList *fcns = rz_anal_get_fcns (c->anal);
        const char *match;
        RzListIter *iter;
        RzAnalFunction *f;
        int maxnamelen = 0;
        ut64 maxsize = 0;
        int digits = 1;
        int len;

        rz_list_foreach (fcns, iter, f) {
                if (f->name && (len = strlen (f->name)) > maxnamelen) {
                        maxnamelen = len;
		}
                if (rz_anal_function_linear_size (f) > maxsize) {
                        maxsize = rz_anal_function_linear_size (f);
		}
        }
        fcns = rz_anal_get_fcns (c2->anal);
        rz_list_foreach (fcns, iter, f) {
                if (f->name && (len = strlen (f->name)) > maxnamelen) {
                        maxnamelen = len;
		}
                if (rz_anal_function_linear_size (f) > maxsize) {
                        maxsize = rz_anal_function_linear_size (f);
		}
        }
        while (maxsize > 9) {
                maxsize /= 10;
                digits++;
        }

        fcns = rz_anal_get_fcns (c->anal);
	if (rz_list_empty (fcns)) {
		eprintf ("No functions found, try running with -A or load a project\n");
		return;
	}
	rz_list_sort (fcns, c->anal->columnSort);

        rz_list_foreach (fcns, iter, f) {
                switch (f->type) {
                case RZ_ANAL_FCN_TYPE_FCN:
                case RZ_ANAL_FCN_TYPE_SYM:
                        switch (f->diff->type) {
                        case RZ_ANAL_DIFF_TYPE_MATCH:
                                match = "MATCH";
                                break;
                        case RZ_ANAL_DIFF_TYPE_UNMATCH:
                                match = "UNMATCH";
                                break;
                        default:
                                match = "NEW";
				f->diff->dist = 0;
                        }
                        diffrow (f->addr, f->name, rz_anal_function_linear_size (f), maxnamelen, digits,
							f->diff->addr, f->diff->name, f->diff->size,
							match, f->diff->dist, bare);
                        break;
                }
        }
        fcns = rz_anal_get_fcns (c2->anal);
	rz_list_sort (fcns, c2->anal->columnSort);
        rz_list_foreach (fcns, iter, f) {
                switch (f->type) {
                case RZ_ANAL_FCN_TYPE_FCN:
                case RZ_ANAL_FCN_TYPE_SYM:
                        if (f->diff->type == RZ_ANAL_DIFF_TYPE_NULL) {
                                diffrow (f->addr, f->name, rz_anal_function_linear_size (f), maxnamelen,
									digits, f->diff->addr, f->diff->name, f->diff->size,
									"NEW", 0, bare); //f->diff->dist, bare);
			}
			break;
                }
        }
}
