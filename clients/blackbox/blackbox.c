#include "dr_api.h"
#include "drsyms.h"

#include "link_observer.h"
#include "module_observer.h"
#include "crowd_safe_trace.h"
#include "basic_block_observer.h"
#include "basic_block_hashtable.h"
#include "indirect_link_observer.h"
#include "indirect_link_hashtable.h"
#include "network_monitor.h"
#include "crowd_safe_gencode.h"
#include "crowd_safe_util.h"

static void
audit_dispatch(dcontext_t *dcontext)
{
    crowd_safe_dispatch(dcontext);
}

static inline void
check_ibp_return(dcontext_t *dcontext)
{
    DODEBUG({
        ibp_metadata_t *ibp_data = GET_IBP_METADATA(dcontext);
        if (IBP_PATH_IS_PENDING(ibp_data)) {
            CS_ERR("Entering fcache with pending IBP "PX" - "PX"!\n",
                ibp_data->ibp_from_tag, ibp_data->ibp_to_tag);

            ASSERT(!IBP_PATH_IS_PENDING(ibp_data));
        }
    });
}

static void
audit_fcache_enter(dcontext_t *dcontext)
{
    check_ibp_return(dcontext);

# ifdef MONITOR_UNEXPECTED_IBP
    start_fcache_clock(dcontext, false);
# endif
    log_shadow_stack(dcontext, GET_CS_DATA(dcontext), "=frag=");
}

static void
audit_fragment_link(dcontext_t *dcontext, bool direct)
{
    if (direct) {
        /* assuming trace head has already been linked as a bb */
        if (!TEST(FRAG_IS_TRACE, dcontext->last_fragment->flags)
            /* && !TEST(LINK_FRAG_OFFS_AT_END, dcontext->last_exit->flags)*/) {
            byte exit_ordinal = find_direct_link_exit_ordinal(dcontext->last_fragment,
                                                              dcontext->next_tag);
            if (exit_ordinal < 0xff) {
                notify_linking_fragments(dcontext, dcontext->last_fragment,
                                         dcontext->next_tag, exit_ordinal);
            }
        }
    } else {
        indirect_link_hashtable_insert(dcontext);
    }
}

static void
audit_fragment_link_tags(dcontext_t *dcontext, app_pc from_tag, app_pc to_tag,
                         byte exit_ordinal)
{
    if (ordinal == 0xff)
        notify_incoming_link(dcontext, from_tag, to_tag);
    else
        notify_linking_fragments(dcontext, from_tag, to_tag, exit_ordinal);
}

static void
audit_syscall(dcontext_t *dcontext, app_pc tag, int syscall_number) {
{
#ifdef MONITOR_UNEXPECTED_IBP
    if (is_stack_spy_sysnum(sysnum)) {
        local_security_audit_state_t *sas = GET_CS_DATA(dcontext);
        if (sas->stack_spy_mark > 0UL && !is_benign_alloc(dcontext)) {
            crowd_safe_thread_local_t *cstl = sas->crowd_safe_thread_local;

            CS_DET("SPY| [0x%llx] Warning: executing syscall 0x%x on a suspicious stack!\n",
                   dr_get_milliseconds(), sysnum);
            CS_DET("SPY| \tNext tag is "PX"\n", dcontext->next_tag);

            write_meta_suspicious_syscall(dcontext, sysnum, &cstl->stack_suspicion);
        }
#endif

    notify_traversing_syscall(dcontext, tag, syscall_number);
}

inline bool
audit_filter_syscall(int sysnum)
{
    return is_stack_spy_sysnum(sysnum); /*do intercept the stack spy sysnums*/
}

#ifdef WINDOWS
static void
audit_callback_context_switch(dcontext_t *dcontext, bool is_return)
{
    log_shadow_stack(dcontext, GET_CS_DATA(dcontext),
                     is_return ? "=callback ret=" "=callback=");
    check_ibp_return(dcontext);
}

static void
audit_nt_continue()
{
#ifdef MONITOR_UNEXPECTED_IBP
    start_fcache_clock(get_thread_private_dcontext(), true);
#endif
}

static void
audit_socket_handle(dcontext_t dcontext, HANDLE handle, bool created)
{
    if (created)
        notify_socket_created(handle);
    else
        socket_handle_remove(dcontext, handle);
}

static void
audit_device_io_control(dcontext_t *dcontext, uint result, HANDLE socket, HANDLE event,
                        IO_STATUS_BLOCK *status_block, IoControlCode control_code,
                        byte *input_data, uint input_length, byte *output_data,
                        uint output_length)
{
    notify_device_io_control(dcontext, result, socket, event, status_block,
                             control_code, input_data, input_length, output_data,
                             output_length);
}

static void
audit_wait_for_single_object(dcontext_t *dcontext, HANDLE event)
{
    notify_wait_for_single_object(dcontext, event);
}

static void
audit_wait_for_multiple_objects(dcontext_t *dcontext, uint result, uint handle_count,
                                HANDLE *handles, bool wait_all)
{
    notify_wait_for_multiple_objects(dcontext, result, handle_count, handles, wait_all);
}
#endif

static void
audit_init_log(bool is_fork, bool is_wow64_process)
{
    init_crowd_safe_log(is_fork, is_wow64_process);
}

static file_t
audit_create_logfile()
{
    create_early_dr_log();
}

static void
audit_close_logfile()
{
    close_crowd_safe_log();
}

static void
audit_init(dcontext_t *dcontext, bool is_fork)
{
    init_link_observer(GLOBAL_DCONTEXT, is_fork);
}

static void
audit_exit()
{
    exit_link_observer();
}

static void
audit_thread_init(dcontext_t *dcontext)
{
    link_observer_thread_init(dcontext);
}

static void
audit_thread_exit(dcontext_t *dcontext)
{
    link_observer_thread_exit(dcontext);
}

static void
audit_process_fork(dcontext_t *dcontext, const wchar_t *name)
{
    notify_process_fork(dcontext, name);
}

static void
audit_dynamo_model_initialized()
{
    notify_dynamo_initialized();
}

static void
audit_close_log()
{
    dr_close_file(early_logfile);
}

static void
audit_bb_link_complete(dcontext_t *dcontext, fragment_t *f)
{
    notify_basic_block_linking_complete(dcontext, f);
}

static void
audit_cache_reset(dcontext_t *dcontext)
{
    notify_cache_reset(dcontext);
}

static void
audit_fragment_remove(dcontext_t *dcontext, fragment_t *f)
{
    if (TEST(FRAG_IS_TRACE, f->flags) && TEST(FRAG_SHARED, f->flags)) {
        CS_DET("Removing trace "PX" with flags 0x%x\n", f->tag, f->flags);
        notify_basic_block_removed(dcontext, f->tag);
    } else if (!(is_live_trace_component && TEST(FRAG_SHARED, f->flags)) &&
               !TEST(FRAG_IS_TRACE, f->flags) && !TEST(FRAG_TEMP_PRIVATE, f->flags)) {
        if (f->also.also_vmarea != NULL)
            CS_WARN("Removing one of multiple versions of BB "PX"!\n", f->tag);
        CS_DET("Removing BB "PX" with flags 0x%x\n", f->tag, f->flags);
        notify_basic_block_removed(dcontext, f->tag);
    }
}

static void
audit_instr(instr_t *instr, byte *copy_pc)
{
    notify_emitting_instruction(instr, copy_pc);
}

static uint
audit_indirect_branchpoint(dcontext_t *dcontext, instrlist_t *ilist, app_pc tag,
                           instr_t *ibl_instr, bool is_return, int syscall_number) {
{
    return insert_indirect_link_branchpoint(dcontext, ilist, tag, ibl_instr,
                                            is_return, syscall_number);
}

static void
audit_instrument_ibl_indirect_handler(dcontext_t *dcontext, instrlist_t *ilist,
                                      app_pc ibl_routine_start_pc,
                                      instr_t *fragment_not_found_handler)
{
    append_indirect_link_notification(dcontext, ilist, ibl_routine_start_pc,
                                      fragment_not_found_handler);
}

static void
audit_instrument_ibl_indirect_hook(dcontext_t *dcontext, instrlist_t *ilist,
                                   app_pc ibl_routine_start_pc)
    append_indirect_link_notification_hook(dcontext, ilist, ibl_routine_start_pc);
}

static void
audit_instrument_ibl_fcache_return(dcontext_t *dcontext, instrlist_t *ilist,
                                   app_pc ibl_routine_start_pc)
{
    prepare_fcache_return_from_ibl(dcontext, ilist, ibl_routine_start_pc);
}

static app_pc
audit_adjust_for_ibl_instrumentation(dcontext_t *dcontext, app_pc pc, app_pc raw_start_pc)
{
    adjust_for_ibl_instrumentation(dcontext, pc, raw_start_pc);
}

static uint
audit_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *next, app_pc tag)
{
    return instrument_return_site(dcontext, ilist, next, tag);
}

static void
audit_process_terminating(bool external, bool is_crash, const char *file,
                          int line, const char *expr)
{
    if (is_crash) {
        CS_LOG("DynamoRIO %s error at %s(%d): %s\n", external ? "external" : "internal",
               file, line, expr);
        CS_STACKTRACE();
    }
    notify_process_terminating(is_crash);
    close_crowd_safe_trace();
}

static void
audit_memory_executable_change(dcontext_t *dcontext, app_pc base, size_t size,
                               bool becomes_executable, bool safe_to_read)
{
    module_location_t *module = get_module_for_address(base);
    if (module != NULL && module->type == module_type_anonymous)) {
        if (becomes_executable)
            add_shadow_pages(dcontext, base, size, true);
        else
            remove_shadow_pages(dcontext, base, size);
    }
}

static void
audit_code_area_expansion(app_pc original_start, app_pc original_end,
                          app_pc new_start, app_pc new_end, bool is_dynamo_areas) {
    code_area_expanded(original_start, original_end, new_start, new_end, is_dynamo_areas);
}

static void
audit_code_area(dcontext_t *dcontext, app_pc start, app_pc end, bool created);
{
    if (created)
        code_area_created(dcontext, start, end);
    else
        memory_released(dcontext, start, end);
}

static void
audit_intercept(app_pc start, app_pc end)
{
    notify_dynamorio_interception(start, end);
}

static void
audit_code_modification(dcontext_t *dcontext, fragment_t *f, app_pc next_pc,
                        app_pc target, size_t write_size)
{
    notify_code_modification(dcontext, f, next_pc, target, write_size);
}

static void
audit_heartbeat(dcontext_t *dcontext)
{
    crowd_safe_heartbeat(dcontext);
}

static void
audit_nested_shadow_stack(dcontext_t *dcontext, bool push)
{
    if (push)
        push_nested_shadow_stack(dcontext);
    else
        pop_nested_shadow_stack(dcontext);
}

static void
audit_gencode_phase(bool start)
{
    if (start)
        notify_gencode_starting();
    else
        notify_gencode_complete();
}

static void
audit_gencode_ibl_routine(app_pc pc, bool syscall)
{
    if (syscall)
        track_shared_syscall_routine(pc);
    else
        track_ibl_routine(pc);
}

static void
audit_translation(dcontext_t *dcontext, app_pc start_pc, instrlist_t *ilist, int sysnum)
{
    if (start_pc != NULL) {
        notify_trace_constructed(dcontext, bb->ilist);
    } else {
        notify_basic_block_constructed(dcontext, start_pc, ilist, sysnum >= 0, sysnum);
    }
}

static void
event_exit(void)
{
    drsym_exit();
}

static audit_callbacks_t callbacks = {
    INVALID_FILE,
    &cs_log_file,
    audit_init,
    audit_exit,
    audit_init_log,
    audit_create_logfile,
    audit_close_logfile,
    audit_dynamo_model_initialized,
    audit_thread_init,
    audit_thread_exit,
    audit_process_fork,
    audit_process_terminating,
    audit_dispatch,
    audit_fcache_enter,
    audit_fragment_link,
    audit_fragment_link_tags,
    audit_syscall,
    audit_filter_syscall,
    audit_bb_link_complete,
    audit_translation,
    audit_fragment_remove,
    audit_cache_reset,
    audit_memory_executable_change,
    audit_code_area_expansion,
    audit_code_area,
    audit_indirect_branchpoint,
    audit_return,
    audit_gencode_phase,
    audit_instr,
    audit_instrument_ibl_indirect_handler,
    audit_instrument_ibl_indirect_hook,
    audit_instrument_ibl_fcache_return,
    audit_code_modification,
    audit_gencode_ibl_routine,
    audit_heartbeat,
    audit_intercept,
    audit_callback_context_switch,
    audit_nested_shadow_stack,
    audit_nt_continue,
    audit_socket_handle,
    audit_device_io_control,
    audit_wait_for_single_object,
    audit_wait_for_multiple_objects
};

#define MAX_MONITOR_DATASET_DIR_LEN 256

uint crowd_safe_options;
const char monitor_dataset_dir[MAX_MONITOR_DATASET_DIR_LEN];

static inline bool
has_option(const char *option)
{
    uint64 option_value;

    return dr_get_integer_option(option, &option_value);
}

DR_EXPORT void
dr_init(client_id_t id)
{
    drsym_init(0);

    dr_register_exit_event(event_exit);
    dr_register_audit_callbacks(&callbacks);

    monitor_dataset_dir = NULL;

    crowd_safe_options |= CROWD_SAFE_BB_GRAPH_OPTION;

    dr_get_string_option("dataset_home",
                         monitor_dataset_dir, MAX_MONITOR_DATASET_DIR_LEN)

    if (has_option("monitor")) {
        crowd_safe_options |= CROWD_SAFE_MONITOR_OPTION;

        /*
        alarm_type = dr_get_integer_option(alarm);
        if (alarm_type > ALARM_OFF) {
#ifdef MONITOR_UNEXPECTED_IBP
            crowd_safe_options |= CROWD_SAFE_ALARM_OPTION;
#else
            CS_ERR("Request for alarm type %d is ignored because unexpected IBP are not monitored in this build. "
                "Requires #define MONITOR_UNEXPECTED_IBP\n", alarm_type);
#endif
        }
        */
    }

    if (has_option("xhash"))
        crowd_safe_options |= CROWD_SAFE_RECORD_XHASH_OPTION;
    if (has_option("netmon"))
        crowd_safe_options |= CROWD_SAFE_NETWORK_MONITOR_OPTION;
    if (has_option("meta_on_clock"))
        crowd_safe_options |= CROWD_SAFE_META_ON_CLOCK_OPTION;

    /*
    OPTION_DEFAULT(uint, bb_analysis_level, 0U,
        "output control for the bb analysis log")
    OPTION_DEFAULT(bool, monitor, false, "monitor control flow")
    OPTION_DEFAULT(pathstring_t, dataset_home, EMPTY_STRING,
        "path to the target's dataset home directory")
    OPTION_DEFAULT(bool, netmon, false, "monitor network")
    OPTION_DEFAULT(bool, xhash, false, "record cross-module hashcodes")
    OPTION_DEFAULT(bool, wdb_script, false, "windbg script")
    OPTION_DEFAULT(bool, meta_on_clock, false, "write metadata on clock tick")
    */
}
