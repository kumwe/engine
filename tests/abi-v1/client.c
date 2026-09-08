/* This consumer includes only the installed C header and the C11 standard library. */
#include <kumwe/engine/engine.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned assertions = 0;
static void check(int condition, const char *message) {
    ++assertions;
    if (!condition) {
        fprintf(stderr, "Installed C consumer: %s\n", message);
        exit(EXIT_FAILURE);
    }
}
static kumwe_engine_v1_view bytes_view(const void *bytes, size_t size) {
    kumwe_engine_v1_view result = {
        (uint32_t)sizeof(kumwe_engine_v1_view), UINT32_C(1), (const uint8_t *)bytes, (uint64_t)size
    };
    return result;
}
static kumwe_engine_v1_view buffer_view(kumwe_engine_v1_buffer *buffer) {
    kumwe_engine_v1_view output = bytes_view(NULL, 0);
    check(kumwe_engine_v1_buffer_view(buffer, &output) == KUMWE_ENGINE_V1_OK, "borrow output view");
    check(output.data != NULL && output.size <= SIZE_MAX, "bounded nonempty output view");
    return output;
}
static void exact(kumwe_engine_v1_buffer *buffer, const void *expected, size_t size) {
    const kumwe_engine_v1_view output = buffer_view(buffer);
    check(output.size == (uint64_t)size && memcmp(output.data, expected, size) == 0, "exact output bytes");
}
static void release_buffer(kumwe_engine_v1_buffer **buffer) {
    kumwe_engine_v1_buffer_release(buffer);
    check(*buffer == NULL, "buffer ownership consumed");
    kumwe_engine_v1_buffer_release(buffer);
    check(*buffer == NULL, "repeated buffer cleanup");
}
static int whitespace(unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}
/* Extract only the bounded, unescaped ASCII identity fields in the public capability document. */
static const unsigned char *member(
    const unsigned char *data, size_t size, const char *name, size_t *length
) {
    const size_t name_size = strlen(name);
    size_t index;
    for (index = 0; index < size; ++index) {
        size_t value, end;
        if (size - index < name_size + 2 || data[index] != '"'
            || memcmp(data + index + 1, name, name_size) != 0 || data[index + name_size + 1] != '"') continue;
        value = index + name_size + 2;
        while (value < size && whitespace(data[value])) ++value;
        if (value == size || data[value++] != ':') continue;
        while (value < size && whitespace(data[value])) ++value;
        if (value == size || data[value++] != '"') continue;
        end = value;
        while (end < size && data[end] != '"' && data[end] != '\\') ++end;
        if (end == size || data[end] != '"') return NULL;
        *length = end - value;
        return data + value;
    }
    return NULL;
}
static void digest_bytes(const unsigned char *value, size_t length, char output[65]) {
    size_t index;
    check(value != NULL && length == 64, "public corpus digest width");
    for (index = 0; index < length; ++index) {
        check((value[index] >= '0' && value[index] <= '9')
            || (value[index] >= 'a' && value[index] <= 'f'), "public corpus digest grammar");
    }
    memcpy(output, value, 64);
    output[64] = '\0';
}
static void profile_digest(const kumwe_engine_v1_view *manifest, const char *profile, char output[65]) {
    const unsigned char *cursor = manifest->data;
    size_t remaining = (size_t)manifest->size;
    const size_t profile_size = strlen(profile);
    while (remaining != 0) {
        size_t length = 0;
        const unsigned char *value = member(cursor, remaining, "profile", &length);
        const unsigned char *end;
        size_t consumed;
        check(value != NULL, "required profile advertised by installed runtime");
        consumed = (size_t)(value - cursor) + length + 1;
        check(consumed <= remaining, "profile field stays in borrowed view");
        if (length == profile_size && memcmp(value, profile, length) == 0) {
            const unsigned char *begin = value;
            /* Contract identity objects contain only scalar metadata; JSON member order is irrelevant. */
            while (begin > manifest->data && begin[-1] != '{') --begin;
            check(begin > manifest->data, "complete profile identity start");
            end = (const unsigned char *)memchr(cursor + consumed, '}', remaining - consumed);
            check(end != NULL && begin <= end, "complete profile identity object");
            value = member(begin, (size_t)(end - begin), "corpus_digest", &length);
            digest_bytes(value, length, output);
            return;
        }
        cursor += consumed;
        remaining -= consumed;
    }
    check(0, "required profile identity missing");
}
static void little_endian(uint8_t *data, size_t *used, uint64_t value, unsigned width) {
    unsigned index;
    for (index = 0; index < width; ++index) {
        data[(*used)++] = (uint8_t)(value & UINT64_C(255));
        value >>= 8;
    }
}
static void decimal_row(
    uint8_t *data, size_t *used, uint8_t operation, uint8_t precision, uint8_t scale,
    uint8_t right_precision, uint8_t right_scale, uint8_t rounding, const char *left, const char *right
) {
    const size_t left_size = strlen(left), right_size = strlen(right);
    check(left_size <= 68 && right_size <= 68 && *used + 10 + left_size + right_size <= 256,
        "consumer decimal fixture bounds");
    data[(*used)++] = operation; data[(*used)++] = precision; data[(*used)++] = scale;
    data[(*used)++] = right_precision; data[(*used)++] = right_scale; data[(*used)++] = rounding;
    little_endian(data, used, (uint64_t)left_size, 2);
    little_endian(data, used, (uint64_t)right_size, 2);
    memcpy(data + *used, left, left_size); *used += left_size;
    memcpy(data + *used, right, right_size); *used += right_size;
}
static void write_fixture(const char *directory, const char *name, const char *bytes) {
    char path[4096];
    FILE *file;
    const int count = snprintf(path, sizeof(path), "%s/%s", directory, name);
    check(count > 0 && (size_t)count < sizeof(path), "fixture path bounds");
    file = fopen(path, "wb");
    check(file != NULL, "create external CLI fixture");
    check(fputs(bytes, file) >= 0 && fputc('\n', file) != EOF, "write external CLI fixture");
    check(fclose(file) == 0, "close external CLI fixture");
}
int main(int argc, char **argv) {
    static const uint8_t discovery[] = {'K','E','C','1',1,0,0,0,1,0,0,0};
    static const char batch[] =
        "{\"wire_version\":1,\"documents\":["
        "{\"correlation\":\"c.1\",\"fields\":{\"amount\":9007199254740993},\"lines\":{}},"
        "{\"correlation\":\"c.2\",\"fields\":{\"amount\":-4},\"lines\":{}}],"
        "\"limits\":{\"max_input_bytes\":10000,\"max_output_bytes\":10000,\"max_documents\":2,"
        "\"max_findings\":10,\"max_instructions\":100000,\"max_milliseconds\":30000}}";
    static const char expected_batch[] =
        "{\"results\":[{\"correlation\":\"c.1\",\"findings\":[],"
        "\"result\":{\"findings\":[],\"value\":9007199254740995},"
        "\"result_json\":\"{\\\"findings\\\":[],\\\"value\\\":9007199254740995}\"},"
        "{\"correlation\":\"c.2\",\"findings\":[],\"result\":{\"findings\":[],\"value\":-2},"
        "\"result_json\":\"{\\\"findings\\\":[],\\\"value\\\":-2}\"}],\"wire_version\":1}";
    static const uint8_t expected_decimal[] = {
        'K','E','R','1',4,0,0,0,4,0,0,0,'1','.','2','0',2,0,0,0,'-','1',
        4,0,0,0,'2','.','4','0',3,0,0,0,'1','.','2'
    };
    static const char canonical_format[] =
        "{\"wire_version\":1,\"profile\":\"kumwe-canonical-json/generic-v1\","
        "\"corpus_digest\":\"%s\",\"operation\":\"%s\",\"input\":{\"type\":\"float\",\"hex\":\"%s\"}}";
    static const char expected_canonical[] = "{\"output\":\"-0.0\"}";
    static const char expected_digest[] =
        "{\"sha256\":\"c26617c7ccbcaa6631b45d851b8cf56e21d2ca624bdb1193afdbd4b560702cec\"}";
    static const char expected_finding[] = "{\"finding\":\"canonical.non-finite-number\"}";
    char formula_digest[65], canonical_digest[65], decimal_digest[65];
    char compile[1024], descriptor[1024], canonical[512], canonical_fixture[512];
    uint8_t decimal[256];
    size_t used = 0, digest_size = 0;
    int count;
    kumwe_engine_v1_buffer *buffer = NULL;
    kumwe_engine_v1_plan *plan = NULL;
    kumwe_engine_v1_cancellation *cancellation = NULL;
    kumwe_engine_v1_view input = bytes_view(discovery, sizeof(discovery));
    kumwe_engine_v1_view output;
    const unsigned char *decimal_identity;
    check(argc == 1 || argc == 2, "optional external fixture directory argument");
    check(sizeof(input) == 24 && offsetof(kumwe_engine_v1_view, data) == 8
        && offsetof(kumwe_engine_v1_view, size) == 16, "public 64-bit ABI layout");
    check(kumwe_engine_v1_capabilities(&input, &buffer) == KUMWE_ENGINE_V1_OK, "discover installed capabilities");
    output = buffer_view(buffer);
    profile_digest(&output, "formula-draft/1", formula_digest);
    profile_digest(&output, "kumwe-canonical-json/generic-v1", canonical_digest);
    decimal_identity = member(output.data, (size_t)output.size, "corpus_sha256", &digest_size);
    digest_bytes(decimal_identity, digest_size, decimal_digest);
    release_buffer(&buffer);

    memcpy(decimal, "KED1", 4); used = 4;
    little_endian(decimal, &used, 4, 4); little_endian(decimal, &used, 10000, 4);
    little_endian(decimal, &used, 100000, 8);
    decimal_row(decimal, &used, 0, 3, 2, 0, 0, 0, "1.2", "");
    decimal_row(decimal, &used, 1, 3, 2, 3, 2, 0, "1.20", "1.30");
    decimal_row(decimal, &used, 2, 3, 1, 2, 1, 0, "1.2", "2.0");
    decimal_row(decimal, &used, 3, 3, 2, 2, 1, 2, "1.25", "");
    input = bytes_view(decimal, used);
    check(kumwe_engine_v1_decimal_batch(&input, &buffer) == KUMWE_ENGINE_V1_OK, "four-operation decimal batch");
    exact(buffer, expected_decimal, sizeof(expected_decimal));
    release_buffer(&buffer);

    count = snprintf(compile, sizeof(compile),
        "{\"corpus_digest\":\"%s\",\"profile\":\"formula-draft/1\",\"program\":{\"args\":["
        "{\"field\":\"amount\",\"op\":\"field\",\"type\":\"integer\"},"
        "{\"op\":\"literal\",\"type\":\"integer\",\"value\":2}],\"op\":\"add\",\"type\":\"integer\"},"
        "\"wire_version\":1}", formula_digest);
    check(count > 0 && (size_t)count < sizeof(compile), "bounded compile envelope");
    memcpy(descriptor, compile, (size_t)count + 1);
    input = bytes_view(compile, (size_t)count);
    check(kumwe_engine_v1_compile(&input, &plan) == KUMWE_ENGINE_V1_OK && plan != NULL, "compile native plan");
    {
        kumwe_engine_v1_plan *original = plan;
        check(kumwe_engine_v1_compile(&input, &plan) == KUMWE_ENGINE_V1_INVALID_INPUT && plan == original,
            "nonempty plan owner refused unchanged");
    }
    memset(compile, 'x', (size_t)count);
    check(kumwe_engine_v1_plan_describe(plan, &buffer) == KUMWE_ENGINE_V1_OK, "describe independent plan");
    exact(buffer, descriptor, strlen(descriptor));
    release_buffer(&buffer);
    check(kumwe_engine_v1_cancellation_create(&cancellation) == KUMWE_ENGINE_V1_OK && cancellation != NULL,
        "create cancellation owner");
    input = bytes_view(batch, sizeof(batch) - 1);
    check(kumwe_engine_v1_execute(plan, &input, cancellation, &buffer) == KUMWE_ENGINE_V1_OK,
        "execute ordered batch with unrequested cancellation");
    exact(buffer, expected_batch, sizeof(expected_batch) - 1);
    release_buffer(&buffer);
    kumwe_engine_v1_cancellation_request(cancellation);
    check(kumwe_engine_v1_execute(plan, &input, cancellation, &buffer) == KUMWE_ENGINE_V1_CANCELLED
        && buffer == NULL, "cancelled batch has no partial result");
    check(kumwe_engine_v1_execute(plan, &input, NULL, &buffer) == KUMWE_ENGINE_V1_OK, "reuse plan after cancellation");
    exact(buffer, expected_batch, sizeof(expected_batch) - 1);
    release_buffer(&buffer);
    kumwe_engine_v1_cancellation_release(&cancellation);
    kumwe_engine_v1_cancellation_release(&cancellation);
    kumwe_engine_v1_cancellation_release(NULL);
    check(cancellation == NULL, "cancellation cleanup clears owner");
    kumwe_engine_v1_plan_release(&plan);
    kumwe_engine_v1_plan_release(&plan);
    kumwe_engine_v1_plan_release(NULL);
    check(plan == NULL, "plan cleanup clears owner");

    count = snprintf(canonical, sizeof(canonical), canonical_format, canonical_digest, "encode", "8000000000000000");
    check(count > 0 && (size_t)count < sizeof(canonical), "bounded canonical envelope");
    memcpy(canonical_fixture, canonical, (size_t)count + 1);
    input = bytes_view(canonical, (size_t)count);
    check(kumwe_engine_v1_canonical(&input, &buffer) == KUMWE_ENGINE_V1_OK, "encode typed negative zero");
    exact(buffer, expected_canonical, sizeof(expected_canonical) - 1);
    release_buffer(&buffer);
    count = snprintf(canonical, sizeof(canonical), canonical_format, canonical_digest, "digest", "8000000000000000");
    check(count > 0 && (size_t)count < sizeof(canonical), "bounded digest envelope");
    input = bytes_view(canonical, (size_t)count);
    check(kumwe_engine_v1_canonical(&input, &buffer) == KUMWE_ENGINE_V1_OK, "stream native canonical digest");
    exact(buffer, expected_digest, sizeof(expected_digest) - 1);
    release_buffer(&buffer);
    count = snprintf(canonical, sizeof(canonical), canonical_format, canonical_digest, "encode", "7ff0000000000000");
    check(count > 0 && (size_t)count < sizeof(canonical), "bounded semantic refusal envelope");
    input = bytes_view(canonical, (size_t)count);
    check(kumwe_engine_v1_canonical(&input, &buffer) == KUMWE_ENGINE_V1_OK, "semantic finding uses successful transport");
    exact(buffer, expected_finding, sizeof(expected_finding) - 1);
    release_buffer(&buffer);
    input = bytes_view("{", 1);
    check(kumwe_engine_v1_canonical(&input, &buffer) == KUMWE_ENGINE_V1_INVALID_INPUT && buffer == NULL,
        "malformed transport refuses without partial buffer");
    kumwe_engine_v1_buffer_release(NULL);
    if (argc == 2) {
        write_fixture(argv[1], "compile.json", descriptor);
        write_fixture(argv[1], "batch.json", batch);
        write_fixture(argv[1], "canonical.json", canonical_fixture);
        write_fixture(argv[1], "canonical-expected.json", expected_canonical);
        write_fixture(argv[1], "batch-expected.json", expected_batch);
    }
    printf("Installed C11 consumer: all 12 ABI exports, %u assertions passed\n", assertions);
    return EXIT_SUCCESS;
}
