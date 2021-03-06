/* **********************************************************
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * bbsize.c
 *
 * Reports basic statistics on the sizes of all basic blocks in the
 * target application.  Illustrates how to preserve floating point
 * state in an event callback.
 */

#include "dr_api.h"

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

static void *stats_mutex; /* for multithread support */
static int num_bb;
static double ave_size;
static int max_size;

static void event_exit(void);
static dr_emit_flags_t event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating);

DR_EXPORT void 
dr_init(client_id_t id)
{
    num_bb = 0;
    ave_size = 0.;
    max_size = 0;
    stats_mutex = dr_mutex_create();
    dr_register_bb_event(event_basic_block);
    dr_register_exit_event(event_exit);
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client bbsize is running\n");
    }
#endif
}

static void 
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[512];
    int len;
    /* Note that using %f with dr_printf or dr_fprintf on Windows will print
     * garbage as they use ntdll._vsnprintf, so we must use dr_snprintf.
     */
    len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                      "Number of basic blocks seen: %d\n"
                      "               Maximum size: %d instructions\n"
                      "               Average size: %5.1f instructions\n",
                      num_bb, max_size, ave_size);
    DR_ASSERT(len > 0);
    msg[sizeof(msg)/sizeof(msg[0])-1] = '\0';
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    dr_mutex_destroy(stats_mutex);
}

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating)
{
    instr_t *instr;
    int cur_size = 0;

    /* we use fp ops so we have to save fp state */
    byte fp_raw[512 + 16];
    byte *fp_align = (byte *) ( (((ptr_uint_t)fp_raw) + 16) & ((ptr_uint_t)-16) );

    if (translating)
        return DR_EMIT_DEFAULT;

    proc_save_fpstate(fp_align);

    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr))
        cur_size++;

    dr_mutex_lock(stats_mutex);
#ifdef VERBOSE_VERBOSE
    dr_fprintf(STDERR,
               "Average: cur=%d, old=%8.1f, num=%d, old*num=%8.1f\n"
               "\told*num+cur=%8.1f, new=%8.1f\n",
               cur_size, ave_size, num_bb, ave_size*num_bb,
               (ave_size * num_bb) + cur_size,
               ((ave_size * num_bb) + cur_size) / (double) (num_bb+1));
#endif
    if (cur_size > max_size)
        max_size = cur_size;
    ave_size = ((ave_size * num_bb) + cur_size) / (double) (num_bb+1);
    num_bb++;
    dr_mutex_unlock(stats_mutex);

    proc_restore_fpstate(fp_align);

    return DR_EMIT_DEFAULT;
}
