/** @file entropy.c 

@author Adam Comley

This file is part of libsqrl.  It is released under the MIT license.
For more details, see the LICENSE file included with this package.
**/

#ifdef _WIN32
#include <Windows.h>
#define SQRL_LOCK_MUTEX(a) EnterCriticalSection(&a->criticalSection)
#define SQRL_UNLOCK_MUTEX(a) LeaveCriticalSection(&a->criticalSection)
#else
#include <pthread.h>
#define SQRL_LOCK_MUTEX(a) pthread_mutex_lock( &a->mutex );
#define SQRL_UNLOCK_MUTEX(a) pthread_mutex_unlock( &a->mutex );
#endif


// fast ~= 50 times per second
// slow ~= 5 times per second
// Depending on processor speed.
#define SQRL_NIX_ENTROPY_REPEAT_FAST 9000000
#define SQRL_NIX_ENTROPY_REPEAT_SLOW 190000000
#define SQRL_WIN_ENTROPY_REPEAT_FAST 9
#define SQRL_WIN_ENTROPY_REPEAT_SLOW 190
#define SQRL_ENTROPY_TARGET 512

#include <sodium.h>

#include "rdrand.h"
#include "../sqrl_internal.h"

struct sqrl_entropy_pool
{
	crypto_hash_sha512_state state;
	int estimated_entropy;
	int entropy_target;
	bool initialized;
	bool stopping;
#ifdef _WIN32
	int sleeptime;
	HANDLE threadHandle;
	CRITICAL_SECTION criticalSection;
#else
	struct timespec sleeptime;
	pthread_t thread;
	pthread_mutex_t mutex;
#endif
};

#if defined(__MACH__)
#include "entropy_mac.h"
#elif defined(_WIN32)
#include "entropy_win.h"
#else
#include "entropy_linux.h"
#endif

struct sqrl_entropy_message
{
	uint8_t *msg;
	struct sqrl_fast_flux_entropy ffe;
};

static struct sqrl_entropy_pool *sqrl_entropy_get_pool();
static void sqrl_increment_entropy( struct sqrl_entropy_pool *pool, int amount );


static void sqrl_entropy_update()
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();
	struct sqrl_fast_flux_entropy ffe;
	sqrl_store_fast_flux_entropy( &ffe );
	SQRL_LOCK_MUTEX(pool);
	crypto_hash_sha512_update( &pool->state, (unsigned char*)&ffe, sizeof( struct sqrl_fast_flux_entropy ));
	sqrl_increment_entropy( pool, 1 );
	SQRL_UNLOCK_MUTEX(pool);
}

#ifdef _WIN32
DWORD WINAPI sqrl_entropy_thread(LPVOID input)
#else
void *sqrl_entropy_thread( void *input )
#endif
{
	struct sqrl_entropy_pool *pool = (struct sqrl_entropy_pool*)input;

#ifndef _WIN32
	struct timespec rtime;
#endif

	while( !pool->stopping ) {
		sqrl_entropy_update();
#ifdef _WIN32
		Sleep( pool->sleeptime );
#else
		if( nanosleep( &pool->sleeptime, &rtime )) {
			// sleep failed... exit so we don't peg processor
			pthread_mutex_lock( &pool->mutex );
			pool->stopping = true;
			pthread_mutex_unlock( &pool->mutex );
		}
#endif
	}
	SQRL_LOCK_MUTEX(pool);
	pool->estimated_entropy = 0;
	pool->initialized = false;
	SQRL_UNLOCK_MUTEX(pool);
#ifdef _WIN32
	ExitThread(0);
#else
	pthread_exit( NULL );
#endif
}

static struct sqrl_entropy_pool *sqrl_entropy_create()
{
	struct sqrl_entropy_pool *pool = malloc( sizeof( struct sqrl_entropy_pool ));
	if( !pool ) {
		return NULL;
	}
	pool->initialized = true;
	pool->stopping = false;
	pool->estimated_entropy = 0;
	pool->entropy_target = SQRL_ENTROPY_TARGET;
	#ifdef _WIN32
	pool->sleeptime = SQRL_WIN_ENTROPY_REPEAT_FAST;
	#else
	pool->sleeptime.tv_sec = 0;
	pool->sleeptime.tv_nsec = SQRL_NIX_ENTROPY_REPEAT_FAST;
	#endif

	crypto_hash_sha512_init( &pool->state );
	sqrl_add_entropy_bracket( pool, NULL );

#ifdef _WIN32
	InitializeCriticalSection(&pool->criticalSection);
	pool->threadHandle = CreateThread(NULL, 0, sqrl_entropy_thread, (void*)pool, 0, NULL);
	
#else
	pthread_mutex_init(&pool->mutex, NULL);

	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	pthread_create( &pool->thread, &attr, sqrl_entropy_thread, (void*)pool );
	pthread_attr_destroy( &attr );
#endif
	return pool;	
}

static struct sqrl_entropy_pool *_public_pool = NULL;

static struct sqrl_entropy_pool *sqrl_entropy_get_pool()
{
	if( _public_pool == NULL ) {
		_public_pool = sqrl_entropy_create();
	}
	return _public_pool;
}


static void sqrl_increment_entropy( struct sqrl_entropy_pool *pool, int amount ) {
	pool->estimated_entropy += amount;
	if( pool->estimated_entropy >= pool->entropy_target ) {
		#ifdef _WIN32
		pool->sleeptime = SQRL_WIN_ENTROPY_REPEAT_SLOW;
		#else
		pool->sleeptime.tv_nsec = SQRL_NIX_ENTROPY_REPEAT_SLOW;
		#endif
	}
}

/**
 * Collects additional entropy.
 *
 * Available entropy is increased by (1 + (\p msg_len / 64))
 *
 * @param msg A chunk of data to be added to the pool
 * @param msg_len The length of \p msg (in bytes)
 */
DLL_PUBLIC
void sqrl_entropy_add( uint8_t* msg, size_t msg_len )
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();
	if( pool->initialized ) {
		SQRL_LOCK_MUTEX(pool);
		if( pool->initialized ) {
			struct sqrl_fast_flux_entropy ffe;
			sqrl_store_fast_flux_entropy( &ffe );
			uint8_t *buf = malloc( msg_len + sizeof( struct sqrl_fast_flux_entropy ));
			if( buf ) {
				memcpy( buf, msg, msg_len );
				memcpy( buf+msg_len, &ffe, sizeof( struct sqrl_fast_flux_entropy ));
				crypto_hash_sha512_update( &pool->state, (unsigned char*)buf, sizeof( buf ));
				sqrl_increment_entropy( pool, 1 + (msg_len / 64) );
				free( buf );
			}
		}
		SQRL_UNLOCK_MUTEX(pool);
	}
}

/**
 * Gets a chunk of entropy, and resets the avaliable entropy counter.  Blocks until \p desired_entropy is available.
 * 
 * @param buf A buffer to receive the entropy.  Must be at least 512 bits (64 bytes) long.
 * @param desired_entropy The minimum amount of estimated entropy required.
 * @return The actual amount of estimated entropy.
 */
DLL_PUBLIC
int sqrl_entropy_get_blocking( uint8_t *buf, int desired_entropy ) 
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();

#ifndef _WIN32
	struct timespec rtime;
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = SQRL_NIX_ENTROPY_REPEAT_SLOW;
#endif
	int received_entropy = 0;
START:
	if( !pool->initialized ) return 0;
	pool->entropy_target = desired_entropy;
	while( pool->estimated_entropy < desired_entropy ) {
#ifdef _WIN32
		Sleep( SQRL_WIN_ENTROPY_REPEAT_SLOW );
#else
		nanosleep( &sleeptime, &rtime );
#endif
	}
	if( pool->initialized &&
		pool->estimated_entropy >= desired_entropy ) {
		SQRL_LOCK_MUTEX(pool);
		if( pool->initialized &&
			pool->estimated_entropy >= desired_entropy ) {
			sqrl_add_entropy_bracket( pool, NULL );
			crypto_hash_sha512_final( &pool->state, buf );
			crypto_hash_sha512_init( &pool->state );
			sqrl_add_entropy_bracket( pool, buf );
			received_entropy = pool->estimated_entropy;
			pool->estimated_entropy = 0;
		} else {
			SQRL_UNLOCK_MUTEX(pool);
			goto START;
		}
		SQRL_UNLOCK_MUTEX(pool);
	} else{
		goto START;
	}
	pool->entropy_target = SQRL_ENTROPY_TARGET;
	#ifdef _WIN32
	pool->sleeptime = SQRL_WIN_ENTROPY_REPEAT_FAST;
	#else
	pool->sleeptime.tv_nsec = SQRL_NIX_ENTROPY_REPEAT_FAST;
	#endif
	return received_entropy;
}

DLL_PUBLIC
int sqrl_entropy_bytes( uint8_t* buf, int nBytes )
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();
	if( !buf || (nBytes <= 0) ) return 0;

	int desired_entropy = (nBytes > 64) ? (8*64) : (8*nBytes);
	if( desired_entropy > SQRL_ENTROPY_NEEDED ) desired_entropy = SQRL_ENTROPY_NEEDED;
	uint8_t tmp[64];
	sodium_mlock( tmp, 64 );
	sqrl_entropy_get_blocking( tmp, desired_entropy );

	if( nBytes <= 64 ) {
		memcpy( buf, tmp, nBytes );
	} else {
		crypto_stream_chacha20( (unsigned char*)buf, nBytes, tmp, (tmp + crypto_stream_chacha20_NONCEBYTES) );
	}

	sodium_munlock( tmp, 64 );
	return nBytes;
}

/**
 * Gets a chunk of entropy, and resets the avaliable entropy counter.
 * 
 * @warning You MUST check the return value before attempting to use \p buf.  If this function returns 0, the buf has NOT been modified, and cannot be trusted as entropy!
 *
 * @param buf A buffer to receive the entropy.  Must be at least 512 bits (64 bytes) long.
 * @param desired_entropy The minimum amount of estimated entropy required.
 * @return The actual amount of estimated entropy.  If \p desired_entropy is not available, returns 0.
 */
DLL_PUBLIC
int sqrl_entropy_get( uint8_t* buf, int desired_entropy )
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();
	int received_entropy = 0;
	if( pool->initialized &&
		pool->estimated_entropy >= desired_entropy ) {
		SQRL_LOCK_MUTEX(pool);
		if( pool->initialized &&
			pool->estimated_entropy >= desired_entropy ) {
			sqrl_add_entropy_bracket( pool, NULL );
			crypto_hash_sha512_final( &pool->state, buf );
			crypto_hash_sha512_init( &pool->state );
			sqrl_add_entropy_bracket( pool, buf );
			received_entropy = pool->estimated_entropy;
			pool->estimated_entropy = 0;
		}
		SQRL_UNLOCK_MUTEX(pool);
	}
	if( pool->estimated_entropy < desired_entropy ) {
		pool->entropy_target = desired_entropy;
	}
	#ifdef _WIN32
	pool->sleeptime = SQRL_WIN_ENTROPY_REPEAT_FAST;
	#else
	pool->sleeptime.tv_nsec = SQRL_NIX_ENTROPY_REPEAT_FAST;
	#endif
	return received_entropy;
}

/**
 * Gets the estimated amount of entropy available in the entropy collection pool.
 *
 * Estimated entropy is not an exact measurement.  It is incremented when additional entropy is collected.  We conservatively assume that each entropy collection contains AT LEAST on bit of real entropy.
 * 
 * @return The estimated entropy (bits) available
 */
DLL_PUBLIC
int sqrl_entropy_estimate()
{
	struct sqrl_entropy_pool *pool = sqrl_entropy_get_pool();
	if( pool->initialized ) {
		return pool->estimated_entropy;
	}
	return 0;
}
