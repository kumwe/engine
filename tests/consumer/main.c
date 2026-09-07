#include <kumwe/engine/engine.h>
#include <stddef.h>
#include <string.h>
int main(void) {
    static const uint8_t request[] = {'K','E','C','1',1,0,0,0,1,0,0,0};
    kumwe_engine_v1_view input = {sizeof(kumwe_engine_v1_view), 1, request, sizeof(request)};
    kumwe_engine_v1_view output = {sizeof(kumwe_engine_v1_view), 1, NULL, 0};
    kumwe_engine_v1_buffer *buffer = NULL;
    if (sizeof(input) != 24 || offsetof(kumwe_engine_v1_view, data) != 8 || offsetof(kumwe_engine_v1_view, size) != 16) return 1;
    if (kumwe_engine_v1_capabilities(&input, &buffer) != 0 || buffer == NULL) return 2;
    if (kumwe_engine_v1_buffer_view(buffer, &output) != 0 || output.size == 0) return 3;
    if (output.data[0] != '{') return 4;
    kumwe_engine_v1_buffer_release(&buffer);
    kumwe_engine_v1_buffer_release(&buffer);
    kumwe_engine_v1_buffer_release(NULL);
    if (buffer != NULL) return 5;
    return 0;
}
