/* Map window stubs. */
#include "map.h"

civ_error_t map_init(void)
{
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t map_wndproc_main(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

void map_shutdown(void)
{
}
