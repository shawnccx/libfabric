/*
 * Copyright (C) 2016 by Argonne National Laboratory.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <pthread.h>

#include "fi_bgq_spi.h"

#include "cq_agent.h"
#include "fi_bgq_memfifo.h"

#define N_PRODUCERS 16
#define N_PACKETS 10000

struct memfifo mfifo;
struct memfifo_producer producer[N_PRODUCERS];
pthread_t info[N_PRODUCERS];

static
void * producer_fn (void *arg) {

	uint16_t id = (uint16_t)((uintptr_t)arg);

	unsigned production_count = N_PACKETS;
	while (production_count > 0) {
		if (0 == memfifo_produce16(&producer[id], id+1)) --production_count;
	}

	return NULL;
}

void test_init_fn (void *buffer, uintptr_t cookie) {

	uint64_t *ptr = (uint64_t *) buffer;
	*ptr = cookie;
}

int main (int argc, char *argv[]) {

	struct l2atomic l2atomic;
	memset((void*)&l2atomic, 0, sizeof(l2atomic));

	int rc, lineno;
	rc = l2atomic_init(&l2atomic); lineno = __LINE__;
	if (rc) goto err;

	struct memfifo mfifo;
	rc = memfifo_initialize(&l2atomic, "some name", &mfifo, 0); lineno = __LINE__;
	if (rc) goto err;

	/* create 'producer' threads */
	uintptr_t pid;
	unsigned production_count[N_PRODUCERS];
	for (pid = 0; pid < N_PRODUCERS; ++pid) {
		producer[pid] = mfifo.producer;
		production_count[pid] = 0;
		if (pthread_create(&info[pid], NULL, &producer_fn, (void*)pid)) { lineno = __LINE__; goto err; }
	}

	unsigned consumption_count = 0;
	unsigned expected_packet_count = N_PRODUCERS * N_PACKETS;
	uint16_t id;
	while (consumption_count < expected_packet_count) {
		if (0 == memfifo_consume16(&mfifo.consumer, &id)) {
			++production_count[id-1];
			++consumption_count;
		}
	}

	for (id = 0; id < N_PRODUCERS; ++id) {
//fprintf(stderr, "%s:%d, production_count[%d]=%d (%d)\n", __FILE__, __LINE__, id, production_count[id], N_PACKETS);
		if (production_count[id] != N_PACKETS) { lineno = __LINE__; goto err; }
	}


	fprintf (stdout, "TEST SUCCESSFUL\n");
	return 0;
err:
	fprintf (stderr, "%s: Error at line %d\n", __FILE__, lineno);
	return 1;
}
