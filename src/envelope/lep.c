#include "../core/internal.h"
#include "laststate/envelope.h"

_Static_assert(LS_PROTOCOL_VERSION == LS_LEP_CURRENT_VERSION,
               "the configured LEP version must match the encoder");

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void mark_truncated(bool *truncated) {
    if (truncated) {
        *truncated = true;
    }
}

static size_t writer_remaining(const ls_writer_t *writer) {
    if (!writer || writer->length > writer->capacity) {
        return 0;
    }
    return writer->capacity - writer->length;
}

#if LS_STORE_STRINGS
static size_t string_value_capacity(const ls_writer_t *writer) {
    size_t remaining = writer_remaining(writer);
    return remaining > 2u ? remaining - 2u : 0u;
}
#endif

static ls_result_t put_string_field(ls_writer_t *writer, uint8_t field, const char *text,
                                    size_t value_limit, bool *truncated) {
    size_t length = ls_string_length(text);
    if (length > 255u) {
        length = 255u;
        mark_truncated(truncated);
    }
    if (length > value_limit) {
        length = value_limit;
        mark_truncated(truncated);
    }

    ls_result_t result = ls_writer_u8(writer, field);
    if (result == LS_OK) {
        result = ls_writer_u8(writer, (uint8_t)length);
    }
    if (result == LS_OK) {
        result = ls_writer_write(writer, text, length);
    }
    return result;
}

static ls_result_t put_optional_tlv(ls_writer_t *writer, uint16_t type, const void *value,
                                    uint16_t length, bool *truncated) {
    ls_result_t result = ls_writer_tlv(writer, type, value, length);
    if (result == LS_ENOSPACE) {
        mark_truncated(truncated);
        return LS_OK;
    }
    return result;
}

static ls_result_t put_identity(ls_writer_t *writer, bool *truncated) {
    const ls_identity_t *identity = ls_runtime.config.identity;
    const char *build = identity->firmware_build_id ? identity->firmware_build_id : ls_build_id();
    const char *fields[] = {
        identity->project_id,
        identity->device_id,
        identity->product,
        identity->hardware_revision,
        identity->bom_revision,
        identity->manufacturing_batch,
        identity->firmware_version,
        build,
        identity->bootloader_version,
        identity->git_commit,
        identity->variant,
        identity->architecture,
        identity->rtos ? identity->rtos : ls_runtime.config.rtos,
        identity->region ? identity->region : ls_runtime.config.region,
        identity->device_group ? identity->device_group : ls_runtime.config.device_group,
    };
    uint8_t value[512];
    size_t outer_capacity = writer_remaining(writer);
    if (outer_capacity < 4u + 30u) {
        return LS_ENOSPACE;
    }

    size_t value_capacity = outer_capacity - 4u;
    if (value_capacity > sizeof(value)) {
        value_capacity = sizeof(value);
    }
    ls_writer_t nested = {value, value_capacity, 0};

    for (uint8_t index = 0; index < 15u; ++index) {
        size_t fields_remaining = (size_t)(14u - index);
        size_t available = writer_remaining(&nested);
        if (available < 2u + fields_remaining * 2u) {
            return LS_ENOSPACE;
        }
        size_t value_limit = available - 2u - fields_remaining * 2u;
        ls_result_t result =
            put_string_field(&nested, (uint8_t)(index + 1u), fields[index], value_limit, truncated);
        if (result != LS_OK) {
            return result;
        }
    }

    return ls_writer_tlv(writer, LS_TLV_IDENTITY, value, (uint16_t)nested.length);
}

static ls_result_t put_reset(ls_writer_t *writer, const ls_event_t *event) {
    uint8_t value[32];
    ls_writer_t nested = {value, sizeof(value), 0};
    ls_result_t result = ls_writer_u8(&nested, (uint8_t)ls_runtime.reset_info.reason);
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.reset_info.raw_reason);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.boot_count);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.reset_info.previous_uptime_ms);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->timestamp_ms);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, ls_runtime.reset_info.expected ? 1u : 0u);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, ls_runtime.previous_crashed ? 1u : 0u);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, ls_runtime.boot_loop ? 1u : 0u);
    }
    if (result != LS_OK) {
        return result;
    }
    return ls_writer_tlv(writer, LS_TLV_RESET, value, (uint16_t)nested.length);
}

static ls_result_t put_event(ls_writer_t *writer, const ls_event_t *event) {
    uint8_t value[64];
    ls_writer_t nested = {value, sizeof(value), 0};
    ls_result_t result = ls_writer_u8(&nested, (uint8_t)event->priority);
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, (uint8_t)event->severity);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, (uint8_t)event->capture_level);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_hash_string(event->domain));
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, (uint32_t)event->code);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->message ? ls_hash_string(event->message) : 0u);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->fingerprint);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->repeat_count);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->first_seen_ms);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, event->last_seen_ms);
    }
    if (result != LS_OK) {
        return result;
    }
    return ls_writer_tlv(writer, LS_TLV_EVENT, value, (uint16_t)nested.length);
}

static ls_result_t put_cpu(ls_writer_t *writer, const ls_arch_context_t *cpu) {
    if (!cpu) {
        return LS_OK;
    }

    uint8_t value[512];
    ls_writer_t nested = {value, sizeof(value), 0};
    ls_result_t result = ls_writer_u8(&nested, (uint8_t)cpu->architecture);
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, (uint8_t)cpu->fault);
    }
    for (unsigned index = 0; index < 32u && result == LS_OK; ++index) {
        result = ls_writer_u32(&nested, cpu->registers[index]);
    }

    const uint32_t special[] = {
        cpu->lr,      cpu->pc,      cpu->xpsr,      cpu->msp,        cpu->psp,    cpu->control,
        cpu->primask, cpu->basepri, cpu->faultmask, cpu->exc_return, cpu->mcause, cpu->mtval,
        cpu->mstatus, cpu->mepc,    cpu->exccause,  cpu->excvaddr,   cpu->ps,     cpu->sar,
    };
    for (size_t index = 0; index < sizeof(special) / sizeof(special[0]) && result == LS_OK;
         ++index) {
        result = ls_writer_u32(&nested, special[index]);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, cpu->has_fpu ? 1u : 0u);
    }
    if (result == LS_OK) {
        result = ls_writer_u8(&nested, cpu->fpu_lazy ? 1u : 0u);
    }
    if (cpu->has_fpu) {
        for (unsigned index = 0; index < 16u && result == LS_OK; ++index) {
            result = ls_writer_u32(&nested, cpu->s[index]);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, cpu->fpscr);
        }
    }
    if (result != LS_OK) {
        return result;
    }
    result = ls_writer_tlv(writer, LS_TLV_CPU, value, (uint16_t)nested.length);
    if (result != LS_OK) {
        return result;
    }

    uint8_t fault[64];
    ls_writer_t fault_writer = {fault, sizeof(fault), 0};
    const uint32_t status[] = {
        cpu->cfsr,
        cpu->hfsr,
        cpu->dfsr,
        cpu->afsr,
        cpu->mmfar,
        cpu->bfar,
        cpu->shcsr,
        cpu->icsr,
        cpu->vtor,
        cpu->sfsr,
        cpu->sfar,
        (uint32_t)cpu->fault_address,
        (uint32_t)cpu->signal_number,
    };
    for (size_t index = 0; index < sizeof(status) / sizeof(status[0]); ++index) {
        result = ls_writer_u32(&fault_writer, status[index]);
        if (result != LS_OK) {
            return result;
        }
    }
    return ls_writer_tlv(writer, LS_TLV_FAULT, fault, (uint16_t)fault_writer.length);
}

static ls_result_t put_breadcrumbs(ls_writer_t *writer, bool *truncated) {
#if LS_ENABLE_BREADCRUMBS
    for (size_t count = 0; count < ls_runtime.breadcrumb_count; ++count) {
        size_t index = (ls_runtime.breadcrumb_next + LS_BREADCRUMB_CAPACITY -
                        ls_runtime.breadcrumb_count + count) %
                       LS_BREADCRUMB_CAPACITY;
        const ls_breadcrumb_record_t *record = &ls_runtime.breadcrumbs[index];
        uint8_t value[160];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u32(&nested, record->at_ms);
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, record->message_id);
        }
        if (result == LS_OK) {
            result = ls_writer_u8(&nested, record->severity);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, record->category_hash);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, record->message_hash);
        }
#if LS_STORE_STRINGS
        if (result == LS_OK) {
            result = put_string_field(&nested, 1u, record->category, string_value_capacity(&nested),
                                      truncated);
        }
        if (result == LS_OK) {
            result = put_string_field(&nested, 2u, record->message, string_value_capacity(&nested),
                                      truncated);
        }
#endif
        if (result == LS_OK) {
            result = ls_writer_u8(&nested, record->value_count);
        }
        for (uint8_t value_index = 0; value_index < record->value_count && result == LS_OK;
             ++value_index) {
            result = ls_writer_u16(&nested, record->values[value_index].key_id);
            if (result == LS_OK) {
                result = ls_writer_u8(&nested, (uint8_t)record->values[value_index].type);
            }
            if (result == LS_OK) {
                result = ls_writer_u32(&nested, record->values[value_index].value.u32);
            }
        }
        if (result == LS_ENOSPACE) {
            mark_truncated(truncated);
            return LS_OK;
        }
        if (result != LS_OK) {
            return result;
        }
        result =
            put_optional_tlv(writer, LS_TLV_BREADCRUMB, value, (uint16_t)nested.length, truncated);
        if (result != LS_OK) {
            return result;
        }
    }
#else
    (void)writer;
    (void)truncated;
#endif
    return LS_OK;
}

static ls_result_t put_metrics(ls_writer_t *writer, bool *truncated) {
#if LS_ENABLE_METRICS
    for (size_t index = 0; index < ls_runtime.metric_count; ++index) {
        const ls_metric_record_t *metric = &ls_runtime.metrics[index];
        uint8_t value[128];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u32(&nested, metric->name_hash);
#if LS_STORE_STRINGS
        if (result == LS_OK) {
            result = put_string_field(&nested, 1u, metric->name, string_value_capacity(&nested),
                                      truncated);
        }
#endif
        if (result == LS_OK) {
            result = ls_writer_u8(&nested, metric->type);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, (uint32_t)metric->value);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, (uint32_t)metric->previous);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, (uint32_t)metric->minimum);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, (uint32_t)metric->maximum);
        }
        if (result == LS_OK) {
            result = ls_writer_u64(&nested, (uint64_t)metric->sum);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, metric->count);
        }
        if (result == LS_OK) {
            result = ls_writer_u8(&nested, metric->window_count);
        }
        for (uint8_t window = 0; window < metric->window_count && result == LS_OK; ++window) {
            result = ls_writer_u32(&nested, (uint32_t)metric->window[window]);
        }
        if (result == LS_ENOSPACE) {
            mark_truncated(truncated);
            return LS_OK;
        }
        if (result != LS_OK) {
            return result;
        }
        result = put_optional_tlv(writer, LS_TLV_METRIC, value, (uint16_t)nested.length, truncated);
        if (result != LS_OK) {
            return result;
        }
    }
#else
    (void)writer;
    (void)truncated;
#endif
    return LS_OK;
}

static ls_result_t put_power_health(ls_writer_t *writer, bool *truncated) {
#if LS_ENABLE_POWER_SAMPLES
    for (size_t count = 0; count < ls_runtime.power_count; ++count) {
        size_t index =
            (ls_runtime.power_next + LS_POWER_SAMPLE_CAPACITY - ls_runtime.power_count + count) %
            LS_POWER_SAMPLE_CAPACITY;
        const ls_power_sample_t *sample = &ls_runtime.power_samples[index];
        uint8_t value[20];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u32(&nested, sample->timestamp_ms);
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, sample->vdd_mv);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, sample->battery_mv);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, (uint16_t)sample->current_ma);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, (uint16_t)sample->temperature_c);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, sample->charger_status);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, sample->power_flags);
        }
        if (result != LS_OK) {
            return result;
        }
        result = put_optional_tlv(writer, LS_TLV_POWER, value, (uint16_t)nested.length, truncated);
        if (result != LS_OK) {
            return result;
        }
    }
#endif

    uint8_t health[32];
    ls_writer_t nested = {health, sizeof(health), 0};
    ls_result_t result = ls_writer_u32(&nested, ls_runtime.watchdog_last_feed);
    if (result == LS_OK) {
        result = ls_writer_u16(&nested, ls_runtime.watchdog_checkpoint);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_hash_string(ls_runtime.active_task));
    }
    if (result != LS_OK) {
        return result;
    }
    return put_optional_tlv(writer, LS_TLV_HEALTH, health, (uint16_t)nested.length, truncated);
}

static ls_result_t put_details(ls_writer_t *writer, const ls_event_t *event, bool *truncated) {
    if (event->assertion) {
        uint8_t value[200];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u32(&nested, (uint32_t)event->assertion->line);
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, ls_hash_string(event->assertion->expression));
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, ls_hash_string(event->assertion->file));
        }
#if LS_STORE_STRINGS
        if (result == LS_OK) {
            result = put_string_field(&nested, 1u, event->assertion->expression,
                                      string_value_capacity(&nested), truncated);
        }
        if (result == LS_OK) {
            result = put_string_field(&nested, 2u, event->assertion->file,
                                      string_value_capacity(&nested), truncated);
        }
        if (result == LS_OK) {
            result = put_string_field(&nested, 3u, event->assertion->message,
                                      string_value_capacity(&nested), truncated);
        }
#endif
        if (result == LS_ENOSPACE) {
            mark_truncated(truncated);
        } else if (result != LS_OK) {
            return result;
        } else {
            result =
                put_optional_tlv(writer, LS_TLV_ASSERT, value, (uint16_t)nested.length, truncated);
            if (result != LS_OK) {
                return result;
            }
        }
    }

    if (event->peripheral) {
        uint8_t value[48];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u8(&nested, (uint8_t)event->peripheral->domain);
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, event->peripheral->fault);
        }
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, event->peripheral->instance);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, event->peripheral->status);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, event->peripheral->address);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, event->peripheral->reg);
        }
        if (result == LS_OK) {
            result = ls_writer_u32(&nested, event->peripheral->timeout_ms);
        }
        for (unsigned index = 0; index < 4u && result == LS_OK; ++index) {
            result = ls_writer_u32(&nested, event->peripheral->auxiliary[index]);
        }
        if (result == LS_ENOSPACE) {
            mark_truncated(truncated);
        } else if (result != LS_OK) {
            return result;
        } else {
            result = put_optional_tlv(writer, LS_TLV_PERIPHERAL, value, (uint16_t)nested.length,
                                      truncated);
            if (result != LS_OK) {
                return result;
            }
        }
    }

    if (event->log) {
        uint8_t value[24];
        ls_writer_t nested = {value, sizeof(value), 0};
        ls_result_t result = ls_writer_u16(&nested, event->log->message_id);
        if (result == LS_OK) {
            result = ls_writer_u16(&nested, event->log->format_id);
        }
        if (result == LS_OK) {
            result = ls_writer_u8(&nested, event->log->argument_count);
        }
        for (uint8_t index = 0; index < event->log->argument_count && result == LS_OK; ++index) {
            result = ls_writer_u32(&nested, event->log->arguments[index]);
        }
        if (result == LS_ENOSPACE) {
            mark_truncated(truncated);
        } else if (result != LS_OK) {
            return result;
        } else {
            result =
                put_optional_tlv(writer, LS_TLV_LOG, value, (uint16_t)nested.length, truncated);
            if (result != LS_OK) {
                return result;
            }
        }
    }

    return LS_OK;
}

static ls_result_t put_memory(ls_writer_t *writer, const ls_event_t *event, bool *truncated) {
#if LS_ENABLE_DUMPS
    if (event->capture_level >= LS_CAPTURE_SELECTIVE) {
        for (size_t index = 0; index < ls_runtime.dump_region_count; ++index) {
            const ls_dump_region_t *region = &ls_runtime.dump_regions[index];
            if (!(region->flags & LS_DUMP_SAFE)) {
                continue;
            }

            size_t length = region->length;
            if (length > LS_DUMP_REGION_MAX_BYTES) {
                length = LS_DUMP_REGION_MAX_BYTES;
                mark_truncated(truncated);
            }
            bool matched = false;
            ls_redaction_mode_t mode = ls_redaction_for(region->address, length, &matched);
            if (matched && mode == LS_REDACT_EXCLUDE) {
                mark_truncated(truncated);
                continue;
            }

            uint8_t value[LS_DUMP_REGION_MAX_BYTES + 48u];
            ls_writer_t nested = {value, sizeof(value), 0};
            ls_result_t result = ls_writer_u32(&nested, ls_hash_string(region->name));
            if (result == LS_OK) {
                result = ls_writer_u32(&nested, (uint32_t)(uintptr_t)region->address);
            }
            if (result == LS_OK) {
                result = ls_writer_u32(&nested, (uint32_t)region->length);
            }
            if (result == LS_OK) {
                result = ls_writer_u32(&nested, region->flags);
            }
            if (result == LS_OK && matched && mode == LS_REDACT_HASH) {
                result = ls_writer_u32(&nested, ls_crc32(region->address, length));
            } else if (result == LS_OK && matched && mode == LS_REDACT_ZERO) {
                uint8_t zero[32] = {0};
                size_t remaining = length;
                while (remaining && result == LS_OK) {
                    size_t chunk = remaining > sizeof(zero) ? sizeof(zero) : remaining;
                    result = ls_writer_write(&nested, zero, chunk);
                    remaining -= chunk;
                }
            } else if (result == LS_OK) {
                result = ls_writer_write(&nested, region->address, length);
            }
            if (result == LS_ENOSPACE) {
                mark_truncated(truncated);
                return LS_OK;
            }
            if (result != LS_OK) {
                return result;
            }
            result =
                put_optional_tlv(writer, LS_TLV_MEMORY, value, (uint16_t)nested.length, truncated);
            if (result != LS_OK) {
                return result;
            }
        }
    }

#if LS_ENABLE_STACK_SNAPSHOT
    if (event->capture_level >= LS_CAPTURE_STACK && event->cpu && ls_runtime.stack_lower &&
        ls_runtime.stack_upper) {
        uintptr_t stack_pointer = event->cpu->psp ? event->cpu->psp : event->cpu->msp;
        if (stack_pointer >= (uintptr_t)ls_runtime.stack_lower &&
            stack_pointer < (uintptr_t)ls_runtime.stack_upper) {
            size_t length = (size_t)((uintptr_t)ls_runtime.stack_upper - stack_pointer);
            if (length > LS_STACK_SNAPSHOT_MAX) {
                length = LS_STACK_SNAPSHOT_MAX;
                mark_truncated(truncated);
            }
            uint8_t value[LS_STACK_SNAPSHOT_MAX + 8u];
            ls_writer_t nested = {value, sizeof(value), 0};
            ls_result_t result = ls_writer_u32(&nested, (uint32_t)stack_pointer);
            if (result == LS_OK) {
                result = ls_writer_u32(&nested, (uint32_t)length);
            }
            if (result == LS_OK) {
                result = ls_writer_write(&nested, (const void *)stack_pointer, length);
            }
            if (result == LS_ENOSPACE) {
                mark_truncated(truncated);
            } else if (result != LS_OK) {
                return result;
            } else {
                result = put_optional_tlv(writer, LS_TLV_STACK, value, (uint16_t)nested.length,
                                          truncated);
                if (result != LS_OK) {
                    return result;
                }
            }
        }
    }
#endif
#else
    (void)event;
#endif

    uint8_t heap[20];
    ls_writer_t nested = {heap, sizeof(heap), 0};
    ls_result_t result = ls_writer_u32(&nested, ls_runtime.heap_stats.free_bytes);
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.heap_stats.minimum_free_bytes);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.heap_stats.largest_block);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.heap_stats.allocation_failures);
    }
    if (result == LS_OK) {
        result = ls_writer_u32(&nested, ls_runtime.heap_stats.pool_exhaustions);
    }
    if (result != LS_OK) {
        return result;
    }
    return put_optional_tlv(writer, LS_TLV_HEAP, heap, (uint16_t)nested.length, truncated);
}

static ls_result_t put_payload(ls_writer_t *writer, const ls_event_t *event, bool *truncated) {
    ls_result_t result = put_identity(writer, truncated);
    if (result == LS_OK) {
        result = put_reset(writer, event);
    }
    if (result == LS_OK) {
        result = put_event(writer, event);
    }
    if (result == LS_OK) {
        result = put_cpu(writer, event->cpu);
    }
    if (result == LS_OK) {
        result = put_breadcrumbs(writer, truncated);
    }
    if (result == LS_OK) {
        result = put_metrics(writer, truncated);
    }
    if (result == LS_OK) {
        result = put_power_health(writer, truncated);
    }
    if (result == LS_OK) {
        result = put_details(writer, event, truncated);
    }
    if (result == LS_OK) {
        result = put_memory(writer, event, truncated);
    }
    return result;
}

static void write_header(uint8_t header[LS_LEP_HEADER_SIZE], uint8_t type, uint8_t architecture,
                         uint8_t flags, uint32_t sequence, uint32_t event_id,
                         uint32_t payload_length) {
    write_u32(header, LS_MAGIC);
    header[4] = LS_LEP_CURRENT_VERSION;
    header[5] = type;
    header[6] = architecture;
    header[7] = flags;
    write_u32(header + 8u, sequence);
    write_u32(header + 12u, event_id);
    write_u32(header + 16u, payload_length);
    write_u32(header + 20u, ls_crc32(header, 20u));
}

ls_result_t ls_envelope_encode(const ls_event_t *event, uint8_t *out, size_t capacity,
                               size_t *length) {
    bool secured = ls_runtime.security_key_length != 0u;
    bool aead = secured && ls_runtime.security_policy.algorithm == LS_SECURITY_XCHACHA20_POLY1305;
    size_t metadata_size = aead ? LS_ENVELOPE_SECURITY_METADATA_SIZE : 0u;
    size_t authentication_size = secured ? (aead ? LS_AEAD_TAG_SIZE : LS_HMAC_SHA256_SIZE) : 0u;
    if (!event || !out || !length ||
        capacity < LS_LEP_HEADER_SIZE + metadata_size + 4u + authentication_size) {
        return LS_EINVAL;
    }

    uint32_t sequence = ++ls_runtime.sequence;
    ls_writer_t payload = {
        out + LS_LEP_HEADER_SIZE + metadata_size,
        capacity - LS_LEP_HEADER_SIZE - metadata_size - 4u - authentication_size,
        0,
    };
    bool truncated = false;
    ls_result_t result = put_payload(&payload, event, &truncated);
    if (result != LS_OK) {
        ls_secure_zero(out, LS_LEP_HEADER_SIZE + metadata_size + payload.length);
        return result;
    }
    if (payload.length > UINT32_MAX) {
        ls_secure_zero(out, LS_LEP_HEADER_SIZE + metadata_size + payload.length);
        return LS_EOVERFLOW;
    }

    uint32_t event_id = ls_crc32(out + LS_LEP_HEADER_SIZE + metadata_size, payload.length) ^
                        sequence ^ ls_hash_string(ls_build_id());
    uint8_t flags = secured ? LS_ENVELOPE_AUTHENTICATED : 0u;
    if (aead) {
        flags |= LS_ENVELOPE_ENCRYPTED | LS_ENVELOPE_AEAD;
    }
    if (truncated) {
        flags |= LS_ENVELOPE_TRUNCATED;
    }

    uint8_t header[LS_LEP_HEADER_SIZE];
    write_header(header, (uint8_t)event->type,
                 (uint8_t)(event->cpu ? event->cpu->architecture : ls_runtime.config.architecture),
                 flags, sequence, event_id, (uint32_t)payload.length);
    ls_memcpy(out, header, sizeof(header));

    if (aead) {
        uint8_t *nonce = out + LS_LEP_HEADER_SIZE;
        result = ls_security_random(nonce, LS_XCHACHA20_NONCE_SIZE);
        if (result != LS_OK) {
            ls_secure_zero(out, LS_LEP_HEADER_SIZE + metadata_size + payload.length);
            return result;
        }
        if (ls_runtime.has_last_envelope_nonce &&
            ls_constant_time_equal(nonce, ls_runtime.last_envelope_nonce,
                                   LS_XCHACHA20_NONCE_SIZE)) {
            ls_secure_zero(out, LS_LEP_HEADER_SIZE + metadata_size + payload.length);
            return LS_EAUTH;
        }

        uint32_t key_id = ls_runtime.security_policy.key_id;
        write_u32(nonce + 24u, key_id);
        uint8_t derived[32];
        static const uint8_t label[] = "laststate/latch/envelope/v1";
        uint8_t salt[12] = {
            (uint8_t)key_id,           (uint8_t)(key_id >> 8),    (uint8_t)(key_id >> 16),
            (uint8_t)(key_id >> 24),   (uint8_t)sequence,         (uint8_t)(sequence >> 8),
            (uint8_t)(sequence >> 16), (uint8_t)(sequence >> 24), (uint8_t)event_id,
            (uint8_t)(event_id >> 8),  (uint8_t)(event_id >> 16), (uint8_t)(event_id >> 24),
        };
        result = ls_hkdf_sha256(salt, sizeof(salt), ls_runtime.security_key,
                                ls_runtime.security_key_length, label, sizeof(label) - 1u, derived,
                                sizeof(derived));
        if (result != LS_OK) {
            ls_secure_zero(out, LS_LEP_HEADER_SIZE + metadata_size + payload.length);
            return result;
        }

        uint8_t *ciphertext = out + LS_LEP_HEADER_SIZE + metadata_size;
        size_t crc_offset = LS_LEP_HEADER_SIZE + metadata_size + payload.length;
        uint8_t *tag = out + crc_offset + 4u;
        result =
            ls_xchacha20_poly1305_encrypt(derived, nonce, out, LS_LEP_HEADER_SIZE + metadata_size,
                                          ciphertext, ciphertext, payload.length, tag);
        ls_secure_zero(derived, sizeof(derived));
        if (result != LS_OK) {
            ls_secure_zero(out, crc_offset + 4u + authentication_size);
            return result;
        }

        uint32_t payload_crc = ls_crc32(out + LS_LEP_HEADER_SIZE, metadata_size + payload.length);
        write_u32(out + crc_offset, payload_crc);
        ls_memcpy(ls_runtime.last_envelope_nonce, nonce, LS_XCHACHA20_NONCE_SIZE);
        ls_runtime.has_last_envelope_nonce = true;
        *length = crc_offset + 4u + authentication_size;
    } else {
        size_t crc_offset = LS_LEP_HEADER_SIZE + payload.length;
        write_u32(out + crc_offset, ls_crc32(out + LS_LEP_HEADER_SIZE, payload.length));
        size_t total = crc_offset + 4u;
        if (authentication_size) {
            ls_hmac_sha256(ls_runtime.security_key, ls_runtime.security_key_length, out, total,
                           out + total);
            total += authentication_size;
        }
        *length = total;
    }

    ls_runtime.persistent.last_sequence = sequence;
    return LS_OK;
}

static bool flags_valid(uint8_t flags) {
    if ((flags & ~LS_ENVELOPE_KNOWN_FLAGS) != 0u) {
        return false;
    }
    bool authenticated = (flags & LS_ENVELOPE_AUTHENTICATED) != 0u;
    bool encrypted = (flags & LS_ENVELOPE_ENCRYPTED) != 0u;
    bool aead = (flags & LS_ENVELOPE_AEAD) != 0u;
    if (encrypted != aead) {
        return false;
    }
    return !aead || authenticated;
}

static ls_result_t validate_payload(const uint8_t *payload, size_t payload_length) {
    size_t offset = 0;
    while (offset < payload_length) {
        if (payload_length - offset < 4u) {
            return LS_ECORRUPT;
        }
        uint16_t type = read_u16(payload + offset);
        uint16_t field_length = read_u16(payload + offset + 2u);
        offset += 4u;
        if (type == 0u || field_length > payload_length - offset) {
            return LS_ECORRUPT;
        }
        offset += field_length;
    }
    return LS_OK;
}

ls_result_t ls_envelope_validate(const uint8_t *data, size_t length, ls_envelope_info_t *info) {
    if (!data) {
        return LS_EINVAL;
    }
    if (length < LS_LEP_HEADER_SIZE + 4u || read_u32(data) != LS_MAGIC) {
        return LS_ECORRUPT;
    }
    if (data[4] < LS_LEP_MIN_SUPPORTED_VERSION || data[4] > LS_LEP_MAX_SUPPORTED_VERSION) {
        return LS_ECORRUPT;
    }

    uint8_t flags = data[7];
    if (!flags_valid(flags)) {
        return LS_ECORRUPT;
    }
    bool encrypted = (flags & LS_ENVELOPE_ENCRYPTED) != 0u;
    size_t metadata_size = encrypted ? LS_ENVELOPE_SECURITY_METADATA_SIZE : 0u;
    size_t authentication_size =
        encrypted ? LS_AEAD_TAG_SIZE
                  : ((flags & LS_ENVELOPE_AUTHENTICATED) ? LS_HMAC_SHA256_SIZE : 0u);
    uint32_t payload_length = read_u32(data + 16u);
    size_t overhead = LS_LEP_HEADER_SIZE + metadata_size + 4u + authentication_size;
    if (length < overhead || payload_length > length - overhead ||
        length != overhead + payload_length) {
        return LS_ECORRUPT;
    }

    size_t crc_offset = LS_LEP_HEADER_SIZE + metadata_size + payload_length;
    if (read_u32(data + 20u) != ls_crc32(data, 20u) ||
        read_u32(data + crc_offset) !=
            ls_crc32(data + LS_LEP_HEADER_SIZE, metadata_size + payload_length)) {
        return LS_ECORRUPT;
    }
    if (!encrypted) {
        ls_result_t result = validate_payload(data + LS_LEP_HEADER_SIZE, payload_length);
        if (result != LS_OK) {
            return result;
        }
    }

    if (info) {
        info->version = data[4];
        info->type = data[5];
        info->architecture = data[6];
        info->flags = flags;
        info->sequence = read_u32(data + 8u);
        info->event_id = read_u32(data + 12u);
        info->payload_length = payload_length;
    }
    return LS_OK;
}

bool ls_envelope_is_truncated(const ls_envelope_info_t *info) {
    return info && (info->flags & LS_ENVELOPE_TRUNCATED) != 0u;
}

void ls_envelope_replay_reset(ls_envelope_replay_t *replay) {
    if (replay) {
        replay->highest_sequence = 0u;
        replay->seen_sequences = 0u;
        replay->initialized = false;
    }
}

bool ls_envelope_replay_accept(ls_envelope_replay_t *replay, uint32_t sequence) {
    if (!replay) {
        return false;
    }
    if (!replay->initialized) {
        replay->highest_sequence = sequence;
        replay->seen_sequences = UINT64_C(1);
        replay->initialized = true;
        return true;
    }

    uint32_t forward = sequence - replay->highest_sequence;
    if (forward != 0u && forward < UINT32_C(0x80000000)) {
        replay->seen_sequences = forward >= LS_ENVELOPE_REPLAY_WINDOW_SIZE
                                     ? UINT64_C(1)
                                     : (replay->seen_sequences << forward) | UINT64_C(1);
        replay->highest_sequence = sequence;
        return true;
    }

    uint32_t distance = replay->highest_sequence - sequence;
    if (distance >= LS_ENVELOPE_REPLAY_WINDOW_SIZE) {
        return false;
    }
    uint64_t bit = UINT64_C(1) << distance;
    if ((replay->seen_sequences & bit) != 0u) {
        return false;
    }
    replay->seen_sequences |= bit;
    return true;
}

static ls_result_t visit_payload(const uint8_t *payload, size_t payload_length,
                                 ls_tlv_visitor_t visitor, void *context) {
    if (!visitor) {
        return LS_EINVAL;
    }
    size_t offset = 0;
    while (offset < payload_length) {
        if (payload_length - offset < 4u) {
            return LS_ECORRUPT;
        }
        uint16_t type = read_u16(payload + offset);
        uint16_t field_length = read_u16(payload + offset + 2u);
        offset += 4u;
        if (type == 0u || field_length > payload_length - offset) {
            return LS_ECORRUPT;
        }
        ls_result_t result = visitor(context, type, payload + offset, field_length);
        if (result != LS_OK) {
            return result;
        }
        offset += field_length;
    }
    return LS_OK;
}

ls_result_t ls_envelope_decrypt_payload(const uint8_t *data, size_t length, uint8_t *plaintext,
                                        size_t capacity, size_t *plaintext_length) {
    if (!plaintext_length) {
        return LS_EINVAL;
    }

    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        return result;
    }
    if (!(info.flags & LS_ENVELOPE_ENCRYPTED) || !(info.flags & LS_ENVELOPE_AEAD) ||
        !ls_runtime.security_key_length) {
        return LS_EAUTH;
    }
    if (plaintext && capacity < info.payload_length) {
        return LS_ENOSPACE;
    }
    if (!plaintext && capacity) {
        return LS_EINVAL;
    }

    const uint8_t *nonce = data + LS_LEP_HEADER_SIZE;
    uint32_t key_id = read_u32(nonce + 24u);
    if (key_id != ls_runtime.security_policy.key_id) {
        return LS_EAUTH;
    }

    uint8_t derived[32];
    static const uint8_t label[] = "laststate/latch/envelope/v1";
    uint32_t sequence = info.sequence;
    uint32_t event_id = info.event_id;
    uint8_t salt[12] = {
        (uint8_t)key_id,           (uint8_t)(key_id >> 8),    (uint8_t)(key_id >> 16),
        (uint8_t)(key_id >> 24),   (uint8_t)sequence,         (uint8_t)(sequence >> 8),
        (uint8_t)(sequence >> 16), (uint8_t)(sequence >> 24), (uint8_t)event_id,
        (uint8_t)(event_id >> 8),  (uint8_t)(event_id >> 16), (uint8_t)(event_id >> 24),
    };
    result =
        ls_hkdf_sha256(salt, sizeof(salt), ls_runtime.security_key, ls_runtime.security_key_length,
                       label, sizeof(label) - 1u, derived, sizeof(derived));
    if (result == LS_OK) {
        const uint8_t *ciphertext = data + LS_LEP_HEADER_SIZE + LS_ENVELOPE_SECURITY_METADATA_SIZE;
        const uint8_t *tag = ciphertext + info.payload_length + 4u;
        result = ls_xchacha20_poly1305_decrypt(
            derived, nonce, data, LS_LEP_HEADER_SIZE + LS_ENVELOPE_SECURITY_METADATA_SIZE,
            ciphertext, plaintext, info.payload_length, tag);
    }
    ls_secure_zero(derived, sizeof(derived));
    if (result == LS_OK) {
        *plaintext_length = info.payload_length;
    } else if (plaintext) {
        ls_secure_zero(plaintext, capacity);
    }
    return result;
}

ls_result_t ls_envelope_visit(const uint8_t *data, size_t length, ls_tlv_visitor_t visitor,
                              void *context) {
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        return result;
    }
    if (info.flags & LS_ENVELOPE_ENCRYPTED) {
        return LS_EAUTH;
    }
    return visit_payload(data + LS_LEP_HEADER_SIZE, info.payload_length, visitor, context);
}

ls_result_t ls_envelope_visit_secure(const uint8_t *data, size_t length, uint8_t *workspace,
                                     size_t workspace_size, ls_tlv_visitor_t visitor,
                                     void *context) {
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        return result;
    }
    if (!(info.flags & LS_ENVELOPE_ENCRYPTED)) {
        if (info.flags & LS_ENVELOPE_AUTHENTICATED) {
            result = ls_envelope_verify_auth(data, length);
            if (result != LS_OK) {
                return result;
            }
        } else if (ls_security_enabled() && ls_runtime.security_policy.reject_plaintext) {
            return LS_EAUTH;
        }
        return visit_payload(data + LS_LEP_HEADER_SIZE, info.payload_length, visitor, context);
    }

    size_t plaintext_length = 0;
    result =
        ls_envelope_decrypt_payload(data, length, workspace, workspace_size, &plaintext_length);
    if (result != LS_OK) {
        return result;
    }
    result = visit_payload(workspace, plaintext_length, visitor, context);
    ls_secure_zero(workspace, plaintext_length);
    return result;
}

ls_result_t ls_envelope_visit_secure_replay(const uint8_t *data, size_t length,
                                            ls_envelope_replay_t *replay, uint8_t *workspace,
                                            size_t workspace_size, ls_tlv_visitor_t visitor,
                                            void *context) {
    if (!replay) {
        return LS_EINVAL;
    }

    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        return result;
    }
    if (!(info.flags & LS_ENVELOPE_ENCRYPTED)) {
        if (info.flags & LS_ENVELOPE_AUTHENTICATED) {
            result = ls_envelope_verify_auth(data, length);
            if (result != LS_OK) {
                return result;
            }
            if (!ls_envelope_replay_accept(replay, info.sequence)) {
                return LS_EAUTH;
            }
        } else if (ls_security_enabled() && ls_runtime.security_policy.reject_plaintext) {
            return LS_EAUTH;
        }
        return visit_payload(data + LS_LEP_HEADER_SIZE, info.payload_length, visitor, context);
    }

    size_t plaintext_length = 0u;
    result =
        ls_envelope_decrypt_payload(data, length, workspace, workspace_size, &plaintext_length);
    if (result != LS_OK) {
        return result;
    }
    result = validate_payload(workspace, plaintext_length);
    if (result == LS_OK && !ls_envelope_replay_accept(replay, info.sequence)) {
        result = LS_EAUTH;
    }
    if (result == LS_OK) {
        result = visit_payload(workspace, plaintext_length, visitor, context);
    }
    ls_secure_zero(workspace, plaintext_length);
    return result;
}
