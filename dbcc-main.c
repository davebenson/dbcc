#include "dsk/dsk-cmdline.h"

... argments
cc
typedef dsk_boolean (*DskCmdlineArgumentHandler) (const char *argument,
                                                  DskError  **error);

int
main(int argc,
     char **argv)
{
  
  dsk_cmdline_init ("umbrella program",
                    "This provides various tools related to dbcc.\n"
                    0);

  dsk_cmdline_begin_mode ("cc");
  dsk_cmdline_try_process_args
  dsk_cmdline_set_argument_handler (argument_handler);
  dsk_cmdline_end_mode ();


}
