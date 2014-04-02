/**
  * Fake getgrnam_r function.
  *
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


#include "getgrnam_r.h"

int call_idx = 0; // Call index that allow
                  // to fake multiple calls to
                  // getgrnam_r

struct faked_getgrnam_call getgrnam_r_responses[PRE_ALLOC_ANSWERS_MAX_IDX];

/**
 * Fake system function and only feed
 * required structure with the one provided
 * in global variable.
 * This is only used for the tests suite.
 */
int getgrnam_r(const char *name, struct group *grp,
                char *buf, size_t buflen, struct group **result) {

    int rc = -1;

    if (call_idx < 0)
        call_idx = 0;

    if (call_idx >= PRE_ALLOC_ANSWERS_MAX_IDX)
        call_idx = 0;

    *grp = *(getgrnam_r_responses[call_idx].getgrnam_grp);
    *result = grp;
    rc = getgrnam_r_responses[call_idx].getgrnam_r_rc;

    call_idx++;

    return rc;
}
