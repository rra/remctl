/*
 * Dummy symbol to prevent an empty library.
 *
 * On platforms that already have all of the functions that libportable would
 * supply, Automake builds an empty library and then calls ar with nonsensical
 * arguments.  Ensure that libportable always contains at least one symbol.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#include <portable/macros.h>

/* Prototype to avoid gcc warnings and set visibility. */
int portable_dummy(void) __attribute__((__visibility__("hidden")));

int
portable_dummy(void)
{
    return 42;
}
