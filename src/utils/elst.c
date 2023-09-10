//  Copyright (c) 2018 Jakub Filipowicz <jakubf@gmail.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#include "elst.h"

#define ELST_USED 0
#define ELST_FREE 1
#define ELST_RESVD_ITEMS 2

struct elst_item {
	unsigned p, n;
	int prio;
	void *ptr;
};

struct elst {
	unsigned capacity;
	unsigned count;
	unsigned hwm;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct elst_item *data;
	elst_data_destructor destructor;
};

void elst_nlock_clear(ELST l);

// -----------------------------------------------------------------------
ELST elst_create(int capacity, elst_data_destructor d)
{
	assert(capacity>0);

	ELST l = (struct elst*) malloc(sizeof(struct elst));
	if (!l) {
		goto cleanup;
	}

	l->data = (struct elst_item*) malloc((capacity+ELST_RESVD_ITEMS) * sizeof(struct elst_item));
	if (!l->data) {
		goto cleanup;
	}

	if (pthread_mutex_init(&l->mutex, NULL)) {
		goto cleanup;
	}

	if (pthread_cond_init(&l->cond, NULL)) {
		pthread_mutex_destroy(&l->mutex);
		goto cleanup;
	}

	l->capacity = capacity;
	l->destructor = d;
	l->count = 0;
	l->hwm = ELST_RESVD_ITEMS;
	// initialize used item list
	l->data[ELST_USED].p = ELST_USED;
	l->data[ELST_USED].n = ELST_USED;
	l->data[ELST_USED].ptr = NULL;
	// initialize free item list
	l->data[ELST_FREE].p = ELST_FREE;
	l->data[ELST_FREE].n = ELST_FREE;
	l->data[ELST_FREE].ptr = NULL;

	return l;

cleanup:
	if (l) free(l->data);
	free(l);
	return NULL;
}

// -----------------------------------------------------------------------
void elst_destroy(ELST l)
{
	if (!l) return;

	elst_clear(l);
	pthread_cond_destroy(&l->cond);
	pthread_mutex_destroy(&l->mutex);
	free(l->data);
	free(l);
}

// -----------------------------------------------------------------------
void elst_nlock_clear(ELST l)
{
	assert(l);
	void *p;
	while ((p = elst_nlock_pop(l))) {
		l->destructor(p);
	}
}

// -----------------------------------------------------------------------
void elst_clear(ELST l)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	elst_nlock_clear(l);
	pthread_mutex_unlock(&l->mutex);
}

// -----------------------------------------------------------------------
int elst_nlock_count(ELST l)
{
	assert(l);

	return l->count;
}

// -----------------------------------------------------------------------
int elst_count(ELST l)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	int count = l->count;
	pthread_mutex_unlock(&l->mutex);

	return count;
}

// -----------------------------------------------------------------------
static inline void __unlink(struct elst_item *d, int i)
{
	int p = d[i].p;
	int n = d[i].n;
	d[p].n = n;
	d[n].p = p;
}

// -----------------------------------------------------------------------
static inline void __link(struct elst_item *d, int i, int p, int n)
{
	d[i].p = p;
	d[i].n = n;
	d[p].n = i;
	d[n].p = i;
}

// -----------------------------------------------------------------------
static int inline __get_free(ELST l)
{
	if (l->count >= l->capacity) {
		return -2;
	}

	struct elst_item *d = l->data;

	int ifree = d[ELST_FREE].n;
	if (ifree == ELST_FREE) {
		ifree = l->hwm;
		l->hwm++;
	} else {
		__unlink(d, ifree);
	}

	return ifree;
}

// -----------------------------------------------------------------------
static inline int __put(ELST l, void *ptr, int prio, int p, int n)
{
	int ifree = __get_free(l);
	if (ifree < 0) return ifree;

	struct elst_item *d = l->data;
	d[ifree].ptr = ptr;
	d[ifree].prio = prio;
	__link(d, ifree, p, n);
	l->count++;
	return l->count;
}

// -----------------------------------------------------------------------
int elst_nlock_append(ELST l, void *ptr)
{
	assert(l);

	return __put(l, ptr, 0, l->data[ELST_USED].p, ELST_USED);
}

// -----------------------------------------------------------------------
int elst_append(ELST l, void *ptr)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	int count = elst_nlock_append(l, ptr);
	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->mutex);
	return count;
}

// -----------------------------------------------------------------------
int elst_nlock_prepend(ELST l, void *ptr)
{
	assert(l);

	return __put(l, ptr, 0, ELST_USED, l->data[ELST_USED].n);
}


// -----------------------------------------------------------------------
int elst_prepend(ELST l, void *ptr)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	int count = elst_nlock_prepend(l, ptr);
	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->mutex);
	return count;
}

// -----------------------------------------------------------------------
int elst_nlock_insert(ELST l, void *ptr, int prio)
{
	assert(l);

	struct elst_item *d = l->data;

	int p = ELST_USED;
	int n = d[ELST_USED].n;
	while ((n != ELST_USED) && (d[n].prio >= prio)) {
		p = n;
		n = d[n].n;
	}

	return __put(l, ptr, prio, p, n);
}

// -----------------------------------------------------------------------
int elst_insert(ELST l, void *ptr, int prio)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	int count = elst_nlock_insert(l, ptr, prio);
	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->mutex);

	return count;
}

// -----------------------------------------------------------------------
void * elst_nlock_pop(ELST l)
{
	assert(l);

	struct elst_item *d = l->data;

	int first = d[ELST_USED].n;
	if (first != ELST_USED) {
		__unlink(d, first);
		__link(d, first, d[ELST_FREE].p, ELST_FREE);
		l->count--;
	}

	return d[first].ptr;
}

// -----------------------------------------------------------------------
void * elst_pop(ELST l)
{
	assert(l);

	pthread_mutex_lock(&l->mutex);
	void *ptr = elst_nlock_pop(l);
	pthread_mutex_unlock(&l->mutex);

	return ptr;
}

// -----------------------------------------------------------------------
void * elst_wait_pop(ELST l, unsigned timeout_ms)
{
	assert(l);

	struct timespec abstime;
	clock_gettime(CLOCK_REALTIME, &abstime);
	long new_nsec = abstime.tv_nsec + timeout_ms * 1000000L;
	abstime.tv_sec += new_nsec / 1000000000L;
	abstime.tv_nsec = new_nsec % 1000000000L;

	pthread_mutex_lock(&l->mutex);

	int res = 0;
	int first;
	struct elst_item *d = l->data;

	while ((first = d[ELST_USED].n) == ELST_USED) {
		if (timeout_ms == 0) {
			pthread_cond_wait(&l->cond, &l->mutex);
		} else {
			res = pthread_cond_timedwait(&l->cond, &l->mutex, &abstime);
			if (res == ETIMEDOUT) break;
		}
		// TODO: error handling
		// TODO: retrigger if not finished
	}

	void *ptr = NULL;
	if (res == 0) {
		ptr = d[first].ptr;
		__unlink(d, first);
		__link(d, first, d[ELST_FREE].p, ELST_FREE);
		l->count--;
	}

	pthread_mutex_unlock(&l->mutex);

	return ptr;
}

// vim: tabstop=4 shiftwidth=4 autoindent
