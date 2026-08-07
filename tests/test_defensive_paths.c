#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "laststate/latch.h"
#include "../src/core/internal.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "defensive paths failed: %s:%d\n", #x, __LINE__);                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    uint16_t status;
    ls_result_t result;
    unsigned calls;
} net_mock_t;

static ls_result_t http_post(void *context, const char *endpoint, const char *content_type,
                             const uint8_t *body, size_t length, uint16_t *status) {
    net_mock_t *mock = (net_mock_t *)context;
    CHECK(endpoint && content_type && body && length && status);
    mock->calls++;
    *status = mock->status;
    return mock->result;
}

static ls_result_t mqtt_publish(void *context, const char *topic, const uint8_t *payload,
                                size_t length, uint8_t qos, bool retain) {
    net_mock_t *mock = (net_mock_t *)context;
    CHECK(topic && payload && length && qos <= 2u && !retain);
    mock->calls++;
    return mock->result;
}

static bool unavailable(void *context) {
    (void)context;
    return false;
}
static size_t zero_mtu(void *context) {
    (void)context;
    return 0;
}
static ls_result_t send_ok(void *context, const uint8_t *data, size_t length) {
    (void)context;
    return data && length ? LS_OK : LS_EINVAL;
}
static ls_result_t send_retry_forever(void *context, const uint8_t *data, size_t length) {
    (void)context;
    (void)data;
    (void)length;
    return LS_EAGAIN;
}
static ls_transport_backend_t *reentrant_transport;
static ls_result_t send_reentrant(void *context, const uint8_t *data, size_t length) {
    (void)context;
    CHECK(ls_transport_send(reentrant_transport, 99u, data, length) == LS_EBUSY);
    return LS_OK;
}
static ls_result_t fragment_retry(void *context, const ls_transport_fragment_t *fragment) {
    unsigned *calls = (unsigned *)context;
    CHECK(fragment && fragment->data && fragment->length);
    (*calls)++;
    return *calls == 1u ? LS_EBUSY : LS_OK;
}

static ls_result_t se_ok_random(void *context, uint8_t *output, size_t length) {
    (void)context;
    if (output) memset(output, 0x5a, length);
    return LS_OK;
}
static ls_result_t se_ok_sign(void *context, const uint8_t digest[32], uint8_t signature[64]) {
    (void)context;
    memcpy(signature, digest, 32);
    memcpy(signature + 32, digest, 32);
    return LS_OK;
}
static ls_result_t se_ok_cert(void *context, uint8_t *output, size_t capacity, size_t *length) {
    (void)context;
    if (capacity < 1u) return LS_ENOSPACE;
    output[0] = 0x30u;
    *length = 1u;
    return LS_OK;
}
static ls_result_t se_ok_derive(void *context, uint32_t key_id, const uint8_t *data, size_t length,
                                uint8_t output[32]) {
    (void)context; (void)data; (void)length;
    memset(output, (int)key_id, 32);
    return LS_OK;
}

static unsigned critical_enters, critical_leaves;
static void enter_critical(void *context) { (void)context; critical_enters++; }
static void leave_critical(void *context) { (void)context; critical_leaves++; }

static int test_util_and_metrics(void) {
    uint8_t a[8] = {0}, b[8] = {0};
    CHECK(ls_memcpy(a, "abcdef", 6) == a);
    CHECK(ls_memcmp(a, "abcdef", 6) == 0);
    b[0] = 1; a[0] = 2; CHECK(ls_memcmp(a, b, 1) > 0);
    a[0] = 0; CHECK(ls_memcmp(a, b, 1) < 0);
    CHECK(ls_string_length(NULL) == 0 && ls_string_length("abc") == 3);
    CHECK(ls_hash_string(NULL) == 0);
    char text[4] = {'x','x','x','x'};
    ls_copy_string(text, sizeof text, "abcdef"); CHECK(!strcmp(text, "abc"));
    ls_copy_string(text, sizeof text, NULL); CHECK(text[0] == '\0');
    ls_copy_string(NULL, 1, "x"); ls_copy_string(text, 0, "x");

    ls_writer_t writer = {b, sizeof b, 0};
    CHECK(ls_writer_write(NULL, a, 1) == LS_ENOSPACE);
    CHECK(ls_writer_write(&writer, NULL, 1) == LS_ENOSPACE);
    writer.length = writer.capacity + 1u; CHECK(ls_writer_write(&writer, a, 0) == LS_ENOSPACE);
    writer.length = 0; CHECK(ls_writer_tlv(&writer, 1u, a, 7u) == LS_ENOSPACE);

    ls_metric_snapshot_t snap;
    CHECK(ls_metric_get(NULL, &snap) == LS_EINVAL);
    CHECK(ls_metric_get("missing", NULL) == LS_EINVAL);
    CHECK(ls_metric_get("missing", &snap) == LS_EINVAL);
    ls_metric_i32("m", 5); ls_metric_i32("m", -2); ls_metric_i32("m", 9);
    CHECK(ls_metric_get("m", &snap) == LS_OK);
    CHECK(snap.current == 9 && snap.previous == -2 && snap.minimum == -2 && snap.maximum == 9);
    CHECK(snap.count == 3 && snap.sum == 12 && snap.average == 4);
    ls_metric_bool("flag", true); CHECK(ls_metric_get("flag", &snap) == LS_OK && snap.current == 1);
    ls_metric_u32("u", 12u); CHECK(ls_metric_get("u", &snap) == LS_OK && snap.current == 12);
    ls_metric_increment("ctr", 2u); ls_metric_increment("ctr", 3u);
    CHECK(ls_metric_get("ctr", &snap) == LS_OK && snap.current == 5 && snap.type == LS_METRIC_COUNTER);
    ls_metrics_reset_window("ctr"); CHECK(ls_metric_get("ctr", &snap) == LS_OK && snap.average == 0);
    ls_metrics_reset_window("missing"); ls_metric_i32(NULL, 1);
    return 0;
}

static int test_breadcrumbs(void) {
    ls_breadcrumb_set_min_level(LS_SEVERITY_WARNING);
    size_t before = ls_runtime.breadcrumb_count;
    ls_breadcrumb("filtered");
    CHECK(ls_runtime.breadcrumb_count == before);
    ls_breadcrumb_kv_t values[LS_BREADCRUMB_KV_MAX + 2u];
    memset(values, 0, sizeof values);
    ls_breadcrumb_t event = {.category="nav", .level=LS_SEVERITY_ERROR, .message_id=7u, .message="fault", .values=values,
                             .value_count=LS_BREADCRUMB_KV_MAX + 2u};
    ls_breadcrumb_event(&event);
    CHECK(ls_runtime.breadcrumb_count == before + 1u);
    CHECK(ls_runtime.breadcrumbs[(ls_runtime.breadcrumb_next + LS_BREADCRUMB_CAPACITY - 1u) % LS_BREADCRUMB_CAPACITY].value_count == LS_BREADCRUMB_KV_MAX);
    ls_breadcrumb_event(NULL);
    ls_breadcrumb_set_min_level(LS_SEVERITY_DEBUG);
    for (size_t i = ls_runtime.breadcrumb_count; i < LS_BREADCRUMB_CAPACITY; ++i) ls_breadcrumb("fill");
    CHECK(ls_runtime.breadcrumb_count == LS_BREADCRUMB_CAPACITY);
    size_t next = ls_runtime.breadcrumb_next;
    ls_breadcrumb_set_policy(LS_BREADCRUMB_DROP_NEWEST); ls_breadcrumb("drop");
    CHECK(ls_runtime.breadcrumb_next == next);
    ls_breadcrumb_set_policy(LS_BREADCRUMB_KEEP_ERRORS);
    ls_breadcrumb_t warning = {.level=LS_SEVERITY_WARNING, .message="warning"};
    ls_breadcrumb_event(&warning); CHECK(ls_runtime.breadcrumb_next == next);
    ls_breadcrumb_event(&event); CHECK(ls_runtime.breadcrumb_next != next);
    return 0;
}

static int test_compression(void) {
    uint8_t out[16] = {0}; size_t n = 0, consumed = 0; uint32_t value = 0; uint32_t vals[2];
    CHECK(ls_varint_u32_encode(1, NULL, 1, &n) == LS_EINVAL);
    CHECK(ls_varint_u32_encode(1, out, 1, NULL) == LS_EINVAL);
    CHECK(ls_varint_u32_encode(128, out, 1, &n) == LS_ENOSPACE);
    CHECK(ls_varint_u32_decode(NULL, 1, &value, &consumed) == LS_EINVAL);
    CHECK(ls_varint_u32_decode(out, 1, NULL, &consumed) == LS_EINVAL);
    CHECK(ls_varint_u32_decode((uint8_t[]){0x80}, 1, &value, &consumed) == LS_EAGAIN);
    CHECK(ls_varint_u32_decode((uint8_t[]){0x80,0x80,0x80,0x80,0x10}, 5, &value, &consumed) == LS_EOVERFLOW);
    CHECK(ls_varint_u32_decode((uint8_t[]){0x80,0x80,0x80,0x80,0x80}, 5, &value, &consumed) == LS_EOVERFLOW);
    CHECK(ls_rle_compress(NULL, 1, out, sizeof out, &n) == LS_EINVAL);
    CHECK(ls_rle_compress((uint8_t[]){1,1,1}, 3, out, 1, &n) == LS_ENOSPACE);
    CHECK(ls_rle_compress((uint8_t[]){1,2}, 2, out, 2, &n) == LS_ENOSPACE);
    CHECK(ls_rle_decompress(NULL, 1, out, sizeof out, &n) == LS_EINVAL);
    CHECK(ls_rle_decompress((uint8_t[]){0x82,1}, 2, out, 2, &n) == LS_ENOSPACE);
    CHECK(ls_rle_decompress((uint8_t[]){0x02,1}, 2, out, sizeof out, &n) == LS_ECORRUPT);
    CHECK(ls_rle_decompress((uint8_t[]){0x01,1,2}, 3, out, 1, &n) == LS_ENOSPACE);
    CHECK(ls_delta_u32_encode(NULL, 1, out, sizeof out, &n) == LS_EINVAL);
    CHECK(ls_delta_u32_encode((uint32_t[]){UINT32_MAX}, 1, out, 0, &n) == LS_ENOSPACE);
    CHECK(ls_delta_u32_decode(NULL, 1, vals, 2, &n) == LS_EINVAL);
    CHECK(ls_delta_u32_decode((uint8_t[]){1}, 1, vals, 0, &n) == LS_ENOSPACE);
    CHECK(ls_delta_u32_decode((uint8_t[]){0x80}, 1, vals, 2, &n) == LS_EAGAIN);
    return 0;
}

static int test_secure_element(void) {
    uint8_t out[64] = {0}, digest[32] = {0}; size_t length = 0;
    ls_secure_element_t e = {0};
    CHECK(ls_secure_element_validate(NULL) == LS_EINVAL);
    CHECK(ls_secure_element_validate(&e) == LS_EINVAL);
    CHECK(ls_secure_element_random(NULL, out, 1) == LS_EINVAL);
    CHECK(ls_secure_element_random(&e, out, 1) == LS_EINVAL);
    e.random = se_ok_random; CHECK(ls_secure_element_random(&e, NULL, 1) == LS_EINVAL);
    CHECK(ls_secure_element_random(&e, NULL, 0) == LS_OK);
    CHECK(ls_secure_element_sign(NULL, digest, out) == LS_EINVAL);
    CHECK(ls_secure_element_sign(&e, digest, out) == LS_EINVAL);
    e.sign_sha256 = se_ok_sign; CHECK(ls_secure_element_sign(&e, NULL, out) == LS_EINVAL);
    CHECK(ls_secure_element_sign(&e, digest, NULL) == LS_EINVAL);
    CHECK(ls_secure_element_certificate(&e, out, sizeof out, &length) == LS_EINVAL);
    e.read_certificate = se_ok_cert;
    CHECK(ls_secure_element_certificate(&e, NULL, sizeof out, &length) == LS_EINVAL);
    CHECK(ls_secure_element_certificate(&e, out, 0, &length) == LS_EINVAL);
    CHECK(ls_secure_element_certificate(&e, out, sizeof out, NULL) == LS_EINVAL);
    CHECK(ls_secure_element_derive(&e, 1, NULL, 1, digest) == LS_EINVAL);
    CHECK(ls_secure_element_derive(&e, 0, NULL, 0, digest) == LS_EINVAL);
    e.derive_key = se_ok_derive; CHECK(ls_secure_element_derive(&e, 1, NULL, 0, NULL) == LS_EINVAL);
    CHECK(ls_secure_element_validate(&e) == LS_OK);
    CHECK(ls_secure_element_derive(&e, 2, NULL, 0, digest) == LS_OK && digest[0] == 2);
    return 0;
}

static int make_envelope(uint8_t *out, size_t capacity, size_t *length) {
    ls_event_t e = {.type=LS_EVENT_MESSAGE, .priority=LS_PRIORITY_WARNING, .severity=LS_SEVERITY_WARNING,
                    .domain="network", .message="test", .timestamp_ms=1u, .capture_level=LS_CAPTURE_METADATA};
    return ls_envelope_encode(&e, out, capacity, length) == LS_OK ? 0 : 1;
}

static int test_network(void) {
    uint8_t env[LS_MAX_EVENT_SIZE], beacon[64]; size_t env_len=0, written=0;
    CHECK(make_envelope(env, sizeof env, &env_len) == 0);
    net_mock_t mock = {200u, LS_OK, 0};
    ls_http_transport_t http = {&mock, "https://unit.invalid", http_post, env_len};
    CHECK(ls_http_transport_send(NULL, env, env_len) == LS_EINVAL);
    ls_http_transport_t bad_http = http; bad_http.post = NULL;
    CHECK(ls_http_transport_send(&bad_http, env, env_len) == LS_EINVAL);
    bad_http = http; bad_http.endpoint = NULL;
    CHECK(ls_http_transport_send(&bad_http, env, env_len) == LS_EINVAL);
    CHECK(ls_http_transport_send(&http, NULL, env_len) == LS_EINVAL);
    CHECK(ls_http_transport_send(&http, env, 0) == LS_EINVAL);
    http.maximum_payload = env_len - 1u; CHECK(ls_http_transport_send(&http, env, env_len) == LS_ENOSPACE);
    http.maximum_payload = 0; CHECK(ls_http_transport_max_payload(&http) == LS_MAX_EVENT_SIZE);
    CHECK(ls_http_transport_max_payload(NULL) == LS_MAX_EVENT_SIZE);
    uint8_t bad[LS_MAX_EVENT_SIZE]; memcpy(bad, env, env_len); bad[0] ^= 1u;
    CHECK(ls_http_transport_send(&http, bad, env_len) != LS_OK);
    mock.result = LS_EIO; CHECK(ls_http_transport_send(&http, env, env_len) == LS_EIO);
    mock.result = LS_OK;
    const uint16_t retry_codes[] = {408u,425u,429u,500u,503u};
    for (size_t i=0;i<sizeof retry_codes/sizeof retry_codes[0];++i) { mock.status=retry_codes[i]; CHECK(ls_http_transport_send(&http, env, env_len)==LS_EAGAIN); }
    mock.status=404u; CHECK(ls_http_transport_send(&http, env, env_len)==LS_EIO);
    mock.status=204u; CHECK(ls_http_transport_send(&http, env, env_len)==LS_OK);

    ls_mqtt_transport_t mqtt = {&mock, "latch/events", mqtt_publish, env_len, 1u};
    CHECK(ls_mqtt_transport_send(NULL, env, env_len)==LS_EINVAL);
    ls_mqtt_transport_t bad_mqtt = mqtt; bad_mqtt.publish = NULL;
    CHECK(ls_mqtt_transport_send(&bad_mqtt, env, env_len)==LS_EINVAL);
    bad_mqtt = mqtt; bad_mqtt.topic = NULL;
    CHECK(ls_mqtt_transport_send(&bad_mqtt, env, env_len)==LS_EINVAL);
    CHECK(ls_mqtt_transport_send(&mqtt, NULL, env_len)==LS_EINVAL);
    CHECK(ls_mqtt_transport_send(&mqtt, env, 0u)==LS_EINVAL);
    mqtt.qos=3u; CHECK(ls_mqtt_transport_send(&mqtt, env, env_len)==LS_EINVAL);
    mqtt.qos=1u; mqtt.maximum_payload=env_len-1u; CHECK(ls_mqtt_transport_send(&mqtt, env, env_len)==LS_ENOSPACE);
    mqtt.maximum_payload=0; CHECK(ls_mqtt_transport_max_payload(&mqtt)==LS_MAX_EVENT_SIZE);
    CHECK(ls_mqtt_transport_max_payload(NULL)==LS_MAX_EVENT_SIZE);
    CHECK(ls_mqtt_transport_send(&mqtt,bad,env_len)!=LS_OK);
    mock.result=LS_EAGAIN; CHECK(ls_mqtt_transport_send(&mqtt,env,env_len)==LS_EAGAIN);
    mock.result=LS_OK; CHECK(ls_mqtt_transport_send(&mqtt,env,env_len)==LS_OK);

    CHECK(ls_incident_beacon_encode(NULL, env_len, 1, 12000, 20, beacon, sizeof beacon, &written)==LS_EINVAL);
    CHECK(ls_incident_beacon_encode(env, env_len, 1, 12000, 20, beacon, 31, &written)==LS_EINVAL);
    CHECK(ls_incident_beacon_encode(bad, env_len, 1, 12000, 20, beacon, sizeof beacon, &written)!=LS_OK);
    CHECK(ls_incident_beacon_encode(env, env_len, 1, 12000, -3, beacon, sizeof beacon, &written)==LS_OK);
    CHECK(written == 32u);
    return 0;
}

static int test_transport_defensive(void) {
    uint8_t data[10] = {0,1,2,3,4,5,6,7,8,9};
    CHECK(ls_transport_result_is_retryable(LS_EAGAIN)); CHECK(ls_transport_result_is_retryable(LS_EBUSY));
    CHECK(!ls_transport_result_is_retryable(LS_EIO));
    CHECK(!ls_transport_can_send(NULL,1));
    ls_transport_backend_t none={0}; CHECK(!ls_transport_can_send(&none,1));
    ls_transport_backend_t off={.send=send_ok,.available=unavailable}; CHECK(!ls_transport_can_send(&off,1));
    ls_transport_backend_t z={.send=send_ok,.max_payload=zero_mtu}; CHECK(!ls_transport_can_send(&z,1));
    CHECK(ls_transport_send(&off,1,data,sizeof data)==LS_EAGAIN);
    CHECK(ls_transport_send(&z,1,data,sizeof data)==LS_EINVAL);
    CHECK(ls_transport_send(&none,1,data,sizeof data)==LS_ENOTSUP);
    ls_transport_backend_t tiny={.send=send_ok,.max_payload=zero_mtu};
    (void)tiny;
    ls_transport_backend_t retry={.send=send_retry_forever,.retry_limit=2};
    CHECK(ls_transport_send(&retry,1,data,sizeof data)==LS_EAGAIN);
    ls_transport_backend_t reentrant={.send=send_reentrant}; reentrant_transport=&reentrant;
    CHECK(ls_transport_send(&reentrant,1,data,sizeof data)==LS_OK);

    unsigned calls=0; ls_transport_backend_t frag={.send_fragment_v2=fragment_retry,.context=&calls,
        .capabilities=LS_TRANSPORT_FRAGMENT,.retry_limit=1};
    /* no max_payload -> event size means unfragmented but fragment-only backends are supported */
    CHECK(ls_transport_send(&frag,2,data,sizeof data)==LS_OK && calls==2u);

    uint8_t target[10]={0}, bits[2]={0}; size_t length=99;
    ls_transport_reassembly_t r;
    ls_transport_reassembly_init(NULL,target,sizeof target,bits,sizeof bits);
    ls_transport_reassembly_init(&r,target,sizeof target,bits,sizeof bits);
    CHECK(ls_transport_reassembly_push(&r,NULL,&length)==LS_EINVAL);
    CHECK(ls_transport_reassembly_push(&r,(ls_transport_fragment_t[]){0},&length)==LS_EINVAL);
    ls_transport_fragment_t f={.event_id=1,.envelope_crc=ls_crc32(data,sizeof data),.total_length=10,
        .fragment_capacity=4,.fragment_crc=ls_crc32(data,4),.fragment_index=0,.fragment_count=3,
        .data=data,.length=4};
    CHECK(ls_transport_reassembly_push(&r,&f,NULL)==LS_EINVAL);
    f.total_length=11; CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ENOSPACE); f.total_length=10;
    f.fragment_count=17; CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ENOSPACE); f.fragment_count=2;
    CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ECORRUPT); f.fragment_count=3;
    f.offset=1; CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ECORRUPT); f.offset=0;
    f.length=3; CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ECORRUPT); f.length=4;
    f.fragment_crc^=1; CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_ECORRUPT); f.fragment_crc^=1;
    CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_OK && length==0);
    ls_transport_fragment_t other=f; other.event_id=2; CHECK(ls_transport_reassembly_push(&r,&other,&length)==LS_EBUSY);
    CHECK(ls_transport_reassembly_push(&r,&f,&length)==LS_OK); /* identical duplicate */
    uint8_t changed[4]={9,9,9,9}; other=f; other.data=changed; other.fragment_crc=ls_crc32(changed,4);
    CHECK(ls_transport_reassembly_push(&r,&other,&length)==LS_ECORRUPT);
    CHECK(!r.active); ls_transport_reassembly_reset(NULL);
    return 0;
}

static int test_policy(void) {
    CHECK(ls_sampling_set_permyriad(NULL,1)==LS_EINVAL);
    CHECK(ls_sampling_set_permyriad("x",10001)==LS_EINVAL);
    CHECK(ls_rate_limit_set(NULL,1,LS_PER_SECOND)==LS_EINVAL);
    CHECK(ls_rate_limit_set("x",0,LS_PER_SECOND)==LS_EINVAL);
    CHECK(ls_rate_limit_set("x",1,(ls_rate_period_t)0)==LS_EINVAL);
    ls_policy_reset();
    for (size_t i=0;i<LS_POLICY_CAPACITY;i++) { char name[16]; snprintf(name,sizeof name,"p%zu",i); CHECK(ls_sampling_set_permyriad(name,10000)==LS_OK); }
    CHECK(ls_sampling_set_permyriad("overflow",10000)==LS_ENOSPACE);
    CHECK(ls_dedup_get(0,NULL)==LS_EINVAL); ls_dedup_snapshot_t ds; CHECK(ls_dedup_get(0,&ds)==LS_EINVAL);
    CHECK(!ls_policy_apply(NULL));
    ls_event_t critical={.type=LS_EVENT_ERROR,.priority=LS_PRIORITY_CRITICAL,.timestamp_ms=1,.domain="c",.code=1,.message="c"};
    CHECK(ls_policy_apply(&critical));
    ls_policy_reset();
    CHECK(ls_rate_limit_set("rate",1,LS_PER_SECOND)==LS_OK);
    ls_event_t e={.type=LS_EVENT_ERROR,.priority=LS_PRIORITY_WARNING,.timestamp_ms=10,.domain="rate",.code=1,.message="x"};
    CHECK(ls_policy_apply(&e)); e.timestamp_ms=11; e.code=2; e.fingerprint=0; CHECK(!ls_policy_apply(&e));
    e.timestamp_ms=2000; e.code=3; e.fingerprint=0; CHECK(ls_policy_apply(&e));
    e.timestamp_ms += LS_DEDUP_WINDOW_MS + 1u; e.fingerprint=critical.fingerprint; CHECK(ls_policy_apply(&e));
    ls_policy_reset();
    return 0;
}

int main(void) {
    static const ls_identity_t identity={.project_id="defensive",.device_id="host",.firmware_build_id="def00001"};
    ls_config_t config={.identity=&identity,.enter_critical=enter_critical,.leave_critical=leave_critical};
    CHECK(ls_init(&config)==LS_OK);
    CHECK(test_util_and_metrics()==0);
    CHECK(test_breadcrumbs()==0);
    CHECK(test_compression()==0);
    CHECK(test_secure_element()==0);
    CHECK(test_network()==0);
    CHECK(test_transport_defensive()==0);
    CHECK(test_policy()==0);
    CHECK(critical_enters && critical_enters==critical_leaves);
    return 0;
}
