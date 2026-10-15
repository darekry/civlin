/* Win16 compat shims (USER/GDI/KERNEL -> SDL3 + stdio).
 *
 * civ.exe is NE 16-bit Protected Mode. Far pointers / selectors have no
 * direct Linux equivalent; this header centralises the mapping so call
 * sites stay close to decompiler shape without Win16 types leaking.
 */
#ifndef CIVLIN_WIN16_COMPAT_H
#define CIVLIN_WIN16_COMPAT_H

#include <stddef.h>
#include <stdint.h>

/* Win16 int/file-handle widths as observed in decompile. */
typedef int16_t win16_int;
typedef int win16_handle; /* _LOPEN/_LREAD handle; -1 == HFILE_ERROR */
typedef uint32_t win16_off; /* file offset for _LLSEEK */

#define WIN16_HFILE_ERROR (-1)

/* Message-loop / WndProc result codes (names mirror USER ordinals). */
typedef enum win16_msg_result {
    WIN16_MSG_HANDLED = 0,
    WIN16_MSG_DEFAULT = 1
} win16_msg_result_t;

#endif /* CIVLIN_WIN16_COMPAT_H */
