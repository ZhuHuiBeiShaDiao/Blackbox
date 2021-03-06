/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef MODULE_H
#define MODULE_H

/* used only in our own routines here which use PF_* converted to MEMPROT_* */
#define OS_IMAGE_READ    (MEMPROT_READ)
#define OS_IMAGE_WRITE   (MEMPROT_WRITE)
#define OS_IMAGE_EXECUTE (MEMPROT_EXEC)

/* i#160/PR 562667: support non-contiguous library mappings.  While we're at
 * it we go ahead and store info on each segment whether contiguous or not.
 */
typedef struct _module_segment_t {
    /* start and end are page-aligned beyond the section alignment */
    app_pc start;
    app_pc end;
    uint prot;
} module_segment_t;

typedef struct _os_module_data_t {
    /* To compute the base address, one determines the memory address associated with
     * the lowest p_vaddr value for a PT_LOAD segment. One then obtains the base
     * address by truncating the memory load address to the nearest multiple of the
     * maximum page size and subtracting the truncated lowest p_vaddr value. 
     * Thus, this is not the load address but the base address used in
     * address references within the file.
     */
    app_pc base_address; 
    size_t alignment; /* the alignment between segments */

    /* Fields for pcaches (PR 295534) */
    size_t checksum;
    size_t timestamp;

    /* i#112: Dynamic section info for exported symbol lookup.  Not
     * using elf types here to avoid having to export those.
     */
    bool   hash_is_gnu;   /* gnu hash function? */
    app_pc hashtab;       /* absolute addr of .hash or .gnu.hash */
    size_t num_buckets;   /* number of bucket entries */
    app_pc buckets;       /* absolute addr of hash bucket table */
    size_t num_chain;     /* number of chain entries */
    app_pc chain;         /* absolute addr of hash chain table */
    app_pc dynsym;        /* absolute addr of .dynsym */
    app_pc dynstr;        /* absolute addr of .dynstr */
    size_t dynstr_size;   /* size of .dynstr */
    size_t symentry_size; /* size of a .dynsym entry */
    /* for .gnu.hash */
    app_pc gnu_bitmask;
    ptr_uint_t gnu_shift;
    ptr_uint_t gnu_bitidx;
    size_t gnu_symbias;   /* .dynsym index of first export */

    /* i#160/PR 562667: support non-contiguous library mappings */
    bool contiguous;
    uint num_segments;   /* number of valid entries in segments array */
    uint alloc_segments; /* capacity of segments array */
    module_segment_t *segments;
} os_module_data_t;

app_pc
module_entry_point(app_pc base, ptr_int_t load_delta);

bool
module_is_header(app_pc base, size_t size /*optional*/);

bool
module_is_partial_map(app_pc base, size_t size, uint memprot);

bool
module_walk_program_headers(app_pc base, size_t view_size, bool at_map,
                            app_pc *out_base, app_pc *out_end, char **out_soname,
                            os_module_data_t *out_data);

uint
module_num_program_headers(app_pc base);

bool
module_file_has_module_header(const char *filename);

bool
module_file_is_module64(file_t f);

bool
module_get_platform(file_t f, dr_platform_t *platform);

/* Redirected functions for loaded module,
 * they are also used by __wrap_* functions in instrument.c
 */

void *
redirect_calloc(size_t nmemb, size_t size);

void *
redirect_malloc(size_t size);

void  
redirect_free(void *ptr);

void *
redirect_realloc(void *ptr, size_t size);

#ifdef LINUX
typedef struct _IO_FILE stdfile_t;
#  define STDFILE_FILENO _fileno
#elif defined(MACOS)
typedef FILE stdfile_t;
#  define STDFILE_FILENO _file
#endif
extern stdfile_t **privmod_stdout;
extern stdfile_t **privmod_stderr;
extern stdfile_t **privmod_stdin;

/* loader.c */
app_pc
get_private_library_address(app_pc modbase, const char *name);

bool
get_private_library_bounds(IN app_pc modbase, OUT byte **start, OUT byte **end);

typedef byte *(*map_fn_t)(file_t f, size_t *size INOUT, uint64 offs,
                          app_pc addr, uint prot/*MEMPROT_*/, map_flags_t map_flags);
typedef bool (*unmap_fn_t)(byte *map, size_t size);
typedef bool (*prot_fn_t)(byte *map, size_t size, uint prot/*MEMPROT_*/);


#ifdef MACOS
/* module_macho.c */
byte *
module_dynamorio_lib_base(void);
#endif


#endif /* MODULE_H */
