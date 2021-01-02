// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Google, Inc.
 *
 * Author:
 *	Sami Tolvanen <samitolvanen@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include "pgo.h"

static int current_node;
static DEFINE_SPINLOCK(pgo_lock);

unsigned long prf_lock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&pgo_lock, flags);

	return flags;
}

void prf_unlock(unsigned long flags)
{
	spin_unlock_irqrestore(&pgo_lock, flags);
}

static struct llvm_prf_value_node *allocate_node(struct llvm_prf_data *p,
						 u32 index, u64 value)
{
	/* Note: caller must hold pgo_lock */
	if (&__llvm_prf_vnds_start[current_node + 1] >= __llvm_prf_vnds_end)
		return NULL; /* Out of nodes */

	current_node++;

	/* Make sure the node is entirely within the section */
	if (&__llvm_prf_vnds_start[current_node] >= __llvm_prf_vnds_end ||
	    &__llvm_prf_vnds_start[current_node + 1] > __llvm_prf_vnds_end)
		return NULL;

	return &__llvm_prf_vnds_start[current_node];
}

void __llvm_profile_instrument_target(u64 target_value, void *data, u32 index)
{
	struct llvm_prf_data *p = (struct llvm_prf_data *)data;
	struct llvm_prf_value_node **counters;
	struct llvm_prf_value_node *curr;
	struct llvm_prf_value_node *min = NULL;
	struct llvm_prf_value_node *prev = NULL;
	u64 min_count = U64_MAX;
	u8 values = 0;
	unsigned long flags;

	if (!p || !p->values)
		return;

	counters = (struct llvm_prf_value_node **)p->values;
	curr = counters[index];

	while (curr) {
		if (target_value == curr->value) {
			curr->count++;
			return;
		}

		if (curr->count < min_count) {
			min_count = curr->count;
			min = curr;
		}

		prev = curr;
		curr = curr->next;
		values++;
	}

	if (values >= LLVM_PRF_MAX_NUM_VALS_PER_SITE) {
		if (!min->count || !(--min->count)) {
			curr = min;
			curr->value = target_value;
			curr->count++;
		}
		return;
	}

	/* Lock when updating the value node structure. */
	flags = prf_lock();

	curr = allocate_node(p, index, target_value);
	if (!curr)
		goto out;

	curr->value = target_value;
	curr->count++;

	if (!counters[index])
		counters[index] = curr;
	else if (prev && !prev->next)
		prev->next = curr;

out:
	prf_unlock(flags);
}
EXPORT_SYMBOL(__llvm_profile_instrument_target);

void __llvm_profile_instrument_range(u64 target_value, void *data,
				     u32 index, s64 precise_start,
				     s64 precise_last, s64 large_value)
{
	if (large_value != S64_MIN && (s64)target_value >= large_value)
		target_value = large_value;
	else if ((s64)target_value < precise_start ||
		 (s64)target_value > precise_last)
		target_value = precise_last + 1;

	__llvm_profile_instrument_target(target_value, data, index);
}
EXPORT_SYMBOL(__llvm_profile_instrument_range);

int InstProfPopcountll(unsigned long long X) {
  // This code originates from https://reviews.llvm.org/rG30626254510f.
  unsigned long long v = X;
  v = v - ((v >> 1) & 0x5555555555555555ULL);
  v = (v & 0x3333333333333333ULL) + ((v >> 2) & 0x3333333333333333ULL);
  v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
  return (int)((unsigned long long)(v * 0x0101010101010101ULL) >> 56);
}

int InstProfClzll(unsigned long long X) {
  unsigned long LeadZeroIdx = 0;

#if !defined(_M_ARM64) && !defined(_M_X64)
  // Scan the high 32 bits.
  if (_BitScanReverse(&LeadZeroIdx, (unsigned long)(X >> 32)))
    return (int)(63 - (LeadZeroIdx + 32)); // Create a bit offset
                                                      // from the MSB.
  // Scan the low 32 bits.
  if (_BitScanReverse(&LeadZeroIdx, (unsigned long)(X)))
    return (int)(63 - LeadZeroIdx);
#else
  if (_BitScanReverse64(&LeadZeroIdx, X)) return 63 - LeadZeroIdx;
#endif
  return 64;
}

uint64_t InstrProfGetRangeRepValue(uint64_t Value) {
  if (Value <= 8)
    // The first ranges are individually tracked. Use the value as is.
    return Value;
  else if (Value >= 513)
    // The last range is mapped to its lowest value.
    return 513;
  else if (InstProfPopcountll(Value) == 1)
    // If it's a power of two, use it as is.
    return Value;
  else
    // Otherwise, take to the previous power of two + 1.
    return (1 << (64 - InstProfClzll(Value) - 1)) + 1;
}

void __llvm_profile_instrument_memop(uint64_t TargetValue, void *Data,
                                uint32_t CounterIndex) {
  // Map the target value to the representative value of its range.
  uint64_t RepValue = InstrProfGetRangeRepValue(TargetValue);
  __llvm_profile_instrument_target(RepValue, Data, CounterIndex);
}
EXPORT_SYMBOL(__llvm_profile_instrument_memop);
