/**
  * This file is part of the remctl project.
  *
  * Copyright 2014 - IN2P3 Computing Centre
  * 
  * IN2P3 Computing Centre - written by <remi.ferrand@cc.in2p3.fr>
  * 
  * This file is governed by the CeCILL  license under French law and
  * abiding by the rules of distribution of free software.  You can  use, 
  * modify and/ or redistribute the software under the terms of the CeCILL
  * license as circulated by CEA, CNRS and INRIA at the following URL
  * "http://www.cecill.info". 
  * 
  * As a counterpart to the access to the source code and  rights to copy,
  * modify and redistribute granted by the license, users are provided only
  * with a limited warranty  and the software's author,  the holder of the
  * economic rights,  and the successive licensors  have only  limited
  * liability. 
  * 
  * In this respect, the user's attention is drawn to the risks associated
  * with loading,  using,  modifying and/or developing or reproducing the
  * software by the user in light of its specific status of free software,
  * that may mean  that it is complicated to manipulate,  and  that  also
  * therefore means  that it is reserved for developers  and  experienced
  * professionals having in-depth computer knowledge. Users are therefore
  * encouraged to load and test the software's suitability as regards their
  * requirements in conditions enabling the security of their systems and/or 
  * data to be ensured and,  more generally, to use and operate it in the 
  * same conditions as regards security. 
  * 
  * The fact that you are presently reading this means that you have had
  * knowledge of the CeCILL license and that you accept its terms.
  *
  * See LICENSE for full licensing terms
  */ 

#ifndef TESTS_SERVER_FAKE_GETGRNAM_R_H
#define TESTS_SERVER_FAKE_GETGRNAM_R_H

#include <stdio.h>
#include <sys/types.h>
#include <grp.h>

#define PRE_ALLOC_ANSWERS_MAX_IDX 5

struct faked_getgrnam_call {
    struct group *getgrnam_grp; // Group struct returned
    int getgrnam_r_rc; // syscall return code
};

/* Call index iterator
 * incremented at every getgrnam_r call
 */
extern int call_idx;

/* Stack of pre-made answers
 * The current answer is decided based on *call_idx* value
 */
extern struct faked_getgrnam_call getgrnam_r_responses[PRE_ALLOC_ANSWERS_MAX_IDX];

int getgrnam_r(const char *name, struct group *grp,
                char *buf, size_t buflen, struct group **result);

#endif
