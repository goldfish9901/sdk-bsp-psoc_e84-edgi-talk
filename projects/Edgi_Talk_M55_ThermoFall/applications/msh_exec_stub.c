#include <rtthread.h>

/*
 * The exam firmware does not execute shell script files.
 * FinSH still references this hook when DFS workdir support is enabled.
 */
__attribute__((weak)) int msh_exec_script(const char *cmd_line, int size)
{
    (void)cmd_line;
    (void)size;
    return -RT_ERROR;
}
