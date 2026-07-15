#ifndef LASTSTATE_ENVELOPE_H
#define LASTSTATE_ENVELOPE_H

#include <stddef.h>
#include <stdint.h>
#include "event.h"
#define LS_LEP_HEADER_SIZE 24u
#define LS_ENVELOPE_AUTHENTICATED 1u
#define LS_ENVELOPE_ENCRYPTED 2u
#define LS_ENVELOPE_AEAD 4u
#define LS_ENVELOPE_SECURITY_METADATA_SIZE 28u
typedef enum {LS_TLV_IDENTITY=1,LS_TLV_RESET,LS_TLV_EVENT,LS_TLV_CPU,LS_TLV_FAULT,LS_TLV_BREADCRUMB,LS_TLV_METRIC,LS_TLV_POWER,LS_TLV_HEALTH,LS_TLV_ASSERT,LS_TLV_PERIPHERAL,LS_TLV_LOG,LS_TLV_MEMORY,LS_TLV_STACK,LS_TLV_HEAP} ls_tlv_type_t;
typedef struct { uint8_t version,type,architecture,flags;uint32_t sequence,event_id,payload_length; } ls_envelope_info_t;
typedef ls_result_t (*ls_tlv_visitor_t)(void *context,uint16_t type,const uint8_t *value,uint16_t length);
ls_result_t ls_envelope_validate(const uint8_t *data,size_t length,ls_envelope_info_t *info);
ls_result_t ls_envelope_visit(const uint8_t *data,size_t length,ls_tlv_visitor_t visitor,void *context);
ls_result_t ls_envelope_decrypt_payload(const uint8_t *data,size_t length,uint8_t *plaintext,size_t capacity,size_t *plaintext_length);
ls_result_t ls_envelope_visit_secure(const uint8_t *data,size_t length,uint8_t *workspace,size_t workspace_size,ls_tlv_visitor_t visitor,void *context);
#endif
