/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef PROGRAM_LOADING_H
#define PROGRAM_LOADING_H

#include <bootmem.h>
#include <commonlib/bsd/cbfs_serialized.h>
#include <commonlib/region.h>
#include <stdint.h>
#include <stddef.h>

enum {
	/* Last segment of program. Can be used to take different actions for
	 * cache maintenance of a program load. */
	SEG_FINAL = 1 << 0,
};

enum prog_type {
	PROG_UNKNOWN,
	PROG_BOOTBLOCK,
	PROG_VERSTAGE,
	PROG_ROMSTAGE,
	PROG_RAMSTAGE,
	PROG_REFCODE,
	PROG_PAYLOAD,
	PROG_BL31,
	PROG_BL32,
	PROG_POSTCAR,
	PROG_OPENSBI,
};

/*
 * prog_segment_loaded() is called for each segment of a program loaded. The
 * SEG_FINAL flag will be set on the last segment loaded. The following two
 * functions, platform_segment_loaded() and arch_segment_loaded(), are called
 * in that order within prog_segment_loaded(). In short, rely on
 * prog_segment_loaded() to perform the proper dispatch sequence.
 */
void prog_segment_loaded(uintptr_t start, size_t size, int flags);
void platform_segment_loaded(uintptr_t start, size_t size, int flags);
void arch_segment_loaded(uintptr_t start, size_t size, int flags);

/* Representation of a program. */
struct prog {
	/* The region_device is the source of program content to load. After
	 * loading program it represents the memory region of the stages and
	 * payload. For architectures that use a bounce buffer
	 * then it would represent the bounce buffer. */
	enum prog_type type;
	enum cbfs_type cbfs_type;
	const char *name;
	struct region_device rdev;
	/* Entry to program with optional argument. It's up to the architecture
	 * to decide if argument is passed. */
	void (*entry)(void *);
	void *arg;
};

#define PROG_INIT(type_, name_)				\
	{						\
		.type = (type_),			\
		.name = (name_),			\
	}

static inline const char *prog_name(const struct prog *prog)
{
	return prog->name;
}

static inline enum prog_type prog_type(const struct prog *prog)
{
	return prog->type;
}

static inline enum cbfs_type prog_cbfs_type(const struct prog *prog)
{
	return prog->cbfs_type;
}

static inline struct region_device *prog_rdev(struct prog *prog)
{
	return &prog->rdev;
}

/* Only valid for loaded programs. */
static inline size_t prog_size(const struct prog *prog)
{
	return region_device_sz(&prog->rdev);
}

/* Only valid for loaded programs. */
static inline void *prog_start(const struct prog *prog)
{
	return rdev_mmap_full(&prog->rdev);
}

static inline void *prog_entry(const struct prog *prog)
{
	return prog->entry;
}

static inline void *prog_entry_arg(const struct prog *prog)
{
	return prog->arg;
}

/* region_device representing the 32-bit flat address space. */
extern const struct mem_region_device addrspace_32bit;

static inline void prog_memory_init(struct prog *prog, uintptr_t ptr,
					size_t size)
{
	rdev_chain(&prog->rdev, &addrspace_32bit.rdev, ptr, size);
}

static inline void prog_set_area(struct prog *prog, void *start, size_t size)
{
	prog_memory_init(prog, (uintptr_t)start, size);
}

static inline void prog_set_entry(struct prog *prog, void *e, void *arg)
{
	prog->entry = e;
	prog->arg = arg;
}

static inline void prog_set_arg(struct prog *prog, void *arg)
{
	prog->arg = arg;
}

/* Locate the identified program to run. Return 0 on success. < 0 on error. */
int prog_locate(struct prog *prog);
/* The prog_locate_hook() is called prior to CBFS traversal. The hook can be
 * used to implement policy that allows or prohibits further progress through
 * prog_locate(). The type and name field within struct prog are the only valid
 * fields. A 0 return value allows further progress while a non-zero return
 * value prohibits further progress */
int prog_locate_hook(struct prog *prog);

/* Run the program described by prog. */
void prog_run(struct prog *prog);
/* Per architecture implementation running a program. */
void arch_prog_run(struct prog *prog);
/* Platform (SoC/chipset) specific overrides for running a program. This is
 * called prior to calling the arch_prog_run. Thus, if there is anything
 * special that needs to be done by the platform similar to the architecture
 * code it needs to that as well. */
void platform_prog_run(struct prog *prog);

struct prog_loader_ops {
	const char *name;
	/* Determine if the loader is the active one. If so returns 1 else 0
	 * or < 0 on error. */
	int (*is_loader_active)(struct prog *prog);
	/* Returns < 0 on error or 0 on success. This function locates
	 * the rdev representing the file data associated with the passed in
	 * prog. */
	int (*locate)(struct prog *prog);
};

/************************
 *   ROMSTAGE LOADING   *
 ************************/

/* Run romstage from bootblock. */
void run_romstage(void);

/* Runtime selector for CBFS_PREFIX of romstage. */
int legacy_romstage_select_and_load(struct prog *romstage);

/************************
 *   RAMSTAGE LOADING   *
 ************************/

/* Run ramstage from romstage. */
void run_ramstage(void);

/***********************
 *   PAYLOAD LOADING   *
 ***********************/

int payload_arch_usable_ram_quirk(uint64_t start, uint64_t size);

/* Load payload into memory in preparation to run. */
void payload_load(void);

/* Run the loaded payload. */
void payload_run(void);

/*
 * selfload() and selfload_check() load payloads into memory.
 * selfload() does not check the payload to see if it targets memory.
 * Call selfload_check() to check that the payload targets usable memory.
 * If it does not, the load will fail and this function
 * will return false. On successful payload loading these functions return true.
 *
 * Defined in src/lib/selfboot.c
 */
bool selfload_check(struct prog *payload, enum bootmem_type dest_type);
bool selfload(struct prog *payload);
/* Like selfload_check() but with the payload data already mapped to memory. */
bool selfload_mapped(struct prog *payload, void *mapping,
		     enum bootmem_type dest_type);

/* Load a FIT payload. The payload data must already be mapped to memory. */
void fit_payload(struct prog *payload, void *data);

#endif /* PROGRAM_LOADING_H */
