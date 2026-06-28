/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _CPUFREQ_EFFIENCY_H
#define _CPUFREQ_EFFIENCY_H

#include <linux/cpufreq.h>

#if IS_ENABLED(CONFIG_OPLUS_FEATURE_SUGOV_POWER_EFFIENCY)
extern unsigned int update_power_effiency_lock(struct cpufreq_policy *policy,
		unsigned int freq, unsigned int loadadj_freq);
extern void frequence_opp_init(struct cpufreq_policy *policy);
#else
static inline unsigned int update_power_effiency_lock(
		struct cpufreq_policy *policy, unsigned int freq,
		unsigned int loadadj_freq)
{
	return freq;
}
static inline void frequence_opp_init(struct cpufreq_policy *policy) { }
#endif

#endif /* _CPUFREQ_EFFIENCY_H */
