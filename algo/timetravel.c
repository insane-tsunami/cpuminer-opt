#include <miner.h>
#include "algo-gate-api.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "algo/blake/sph_blake.h"
#include "algo/bmw/sph_bmw.h"
#include "algo/jh/sph_jh.h"
#include "algo/keccak/sph_keccak.h"
#include "algo/skein/sph_skein.h"
#include "algo/luffa/sph_luffa.h"
#include "algo/luffa/sse2/luffa_for_sse2.h"
#include "algo/cubehash/sse2/cubehash_sse2.h"

#ifdef NO_AES_NI
  #include "algo/groestl/sph_groestl.h"
#else
  #include "algo/groestl/aes_ni/hash-groestl.h"
#endif

// Machinecoin Genesis Timestamp
#define HASH_FUNC_BASE_TIMESTAMP 1389040865

#define HASH_FUNC_COUNT 8
#define HASH_FUNC_COUNT_PERMUTATIONS 40320

//static int permutation[HASH_FUNC_COUNT] = { 0 };
static __thread uint32_t s_ntime = UINT32_MAX;
static __thread int permutation[HASH_FUNC_COUNT] = { 0 };

inline void reverse( int *pbegin, int *pend )
{
   while ( (pbegin != pend) && (pbegin != --pend) )
   {
      swap_vars( *pbegin, *pend );
      pbegin++;
   }
}

static void next_permutation( int *pbegin, int *pend )
{
   if ( pbegin == pend )
	return;

   int *i = pbegin;
   ++i;
   if ( i == pend )
	return;

   i = pend;
   --i;

   while (1)
   {
	int *j = i;
	--i;

	if ( *i < *j )
        {
           int *k = pend;

	   while ( !(*i < *--k) ) /* do nothing */ ;

	   swap_vars( *i, *k );
	   reverse(j, pend);
		return; // true
	}

	if ( i == pbegin )
        {
	   reverse(pbegin, pend);
	   return; // false
	}
        // else?
   }
}

typedef struct {
        sph_blake512_context    blake;
        sph_bmw512_context      bmw;
        sph_skein512_context    skein;
        sph_jh512_context       jh;
        sph_keccak512_context   keccak;
        sph_luffa512_context    luffa;
//        hashState_luffa         luffa;
        cubehashParam           cube;
// ctx optimization doesn't work for groestl, do it the old way
//#ifdef NO_AES_NI
//        sph_groestl512_context  groestl;
//#else
//        hashState_groestl       groestl;
//#endif
} tt_ctx_holder;

tt_ctx_holder tt_ctx;

void init_tt_ctx()
{
        sph_blake512_init( &tt_ctx.blake );
        sph_bmw512_init( &tt_ctx.bmw );
        sph_skein512_init( &tt_ctx.skein );
        sph_jh512_init( &tt_ctx.jh );
        sph_keccak512_init( &tt_ctx.keccak );
        sph_luffa512_init( &tt_ctx.luffa );
//        init_luffa( &tt_ctx.luffa, 512 );
        cubehashInit( &tt_ctx.cube, 512, 16, 32 );
//#ifdef NO_AES_NI
//        sph_groestl512_init( &tt_ctx.groestl );
//#else
//        init_groestl( &tt_ctx.groestl );
//#endif
};

void timetravel_hash(void *output, const void *input)
{
   uint32_t _ALIGN(64) hash[128]; // 16 bytes * HASH_FUNC_COUNT
   uint32_t *hashA, *hashB;
   uint32_t dataLen = 64;
   uint32_t *work_data = (uint32_t *)input;
   const uint32_t timestamp = work_data[17];
   tt_ctx_holder ctx;
   memcpy( &ctx, &tt_ctx, sizeof(tt_ctx) );
   int i;

// workaround for initializing groestl ctx
#ifdef NO_AES_NI
   sph_groestl512_context  ctx_groestl;
#else
   hashState_groestl       ctx_groestl;
#endif

   for ( i = 0; i < HASH_FUNC_COUNT; i++ )
   {
        if (i == 0)
        {
	   dataLen = 80;
	   hashA = work_data;
	}
        else
        {
           dataLen = 64;
	   hashA = &hash[16 * (i - 1)];
	}
	hashB = &hash[16 * i];

	switch ( permutation[i] )
        {
	   case 0:
//		sph_blake512_init( &ctx.blake );
		sph_blake512( &ctx.blake, hashA, dataLen );
		sph_blake512_close( &ctx.blake, hashB );
		break;
	   case 1:
//		sph_bmw512_init( &ctx.bmw );
		sph_bmw512( &ctx.bmw, hashA, dataLen );
		sph_bmw512_close( &ctx.bmw, hashB );
		break;
	   case 2:
#ifdef NO_AES_NI
		sph_groestl512_init( &ctx_groestl );
		sph_groestl512( &ctx_groestl, hashA, dataLen );
		sph_groestl512_close( &ctx_groestl, hashB );
#else
                init_groestl( &ctx_groestl );
                update_groestl( &ctx_groestl, (char*)hashA, dataLen*8 );
                final_groestl( &ctx_groestl, (char*)hashB );
#endif
		break;
	   case 3:
//		sph_skein512_init( &ctx.skein );
		sph_skein512( &ctx.skein, hashA, dataLen );
		sph_skein512_close( &ctx.skein, hashB );
		break;
	   case 4:
//		sph_jh512_init( &ctx.jh );
		sph_jh512( &ctx.jh, hashA, dataLen );
		sph_jh512_close( &ctx.jh, hashB);
		break;
	   case 5:
//		sph_keccak512_init( &ctx.keccak );
		sph_keccak512( &ctx.keccak, hashA, dataLen );
		sph_keccak512_close( &ctx.keccak, hashB );
		break;
	   case 6:
//              sph_luffa512_init( &ctx.luffa );
                sph_luffa512 ( &ctx.luffa, hashA, dataLen );
                sph_luffa512_close( &ctx.luffa, hashB );
//                init_luffa( &ctx.luffa, 512 );
//                update_luffa( &ctx.luffa, (const BitSequence*)hashA, dataLen*8 );
//                final_luffa( &ctx.luffa, (BitSequence*)hashB );
		break;
	   case 7:
//                cubehashInit( &ctx.cube, 512, 16, 32 );
                cubehashUpdate( &ctx.cube, (const byte*) hashA, dataLen );
                cubehashDigest( &ctx.cube, (byte*)hashB );
		break;
	   default:
		break;
	   }
	}

	memcpy(output, &hash[16 * (HASH_FUNC_COUNT - 1)], 32);
}

int scanhash_timetravel( int thr_id, struct work *work, uint32_t max_nonce,
                         uint64_t *hashes_done )
{
	uint32_t _ALIGN(64) hash[8];
	uint32_t _ALIGN(64) endiandata[20];
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;

	const uint32_t Htarg = ptarget[7];
	const uint32_t first_nonce = pdata[19];
	uint32_t nonce = first_nonce;
	volatile uint8_t *restart = &(work_restart[thr_id].restart);
        int i;

	if (opt_benchmark)
		ptarget[7] = 0x0cff;

	for (int k=0; k < 19; k++)
		be32enc(&endiandata[k], pdata[k]);

        const uint32_t timestamp = endiandata[17];
        if ( timestamp != s_ntime )
        {
           const int steps = ( timestamp - HASH_FUNC_BASE_TIMESTAMP )
                         % HASH_FUNC_COUNT_PERMUTATIONS;
           for ( i = 0; i < HASH_FUNC_COUNT; i++ )
              permutation[i] = i;
           for ( i = 0; i < steps; i++ )
              next_permutation( permutation, permutation + HASH_FUNC_COUNT );
           s_ntime = timestamp;
        }

	do {
		be32enc(&endiandata[19], nonce);
		timetravel_hash(hash, endiandata);

		if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
			work_set_target_ratio(work, hash);
			pdata[19] = nonce;
			*hashes_done = pdata[19] - first_nonce;
			return 1;
		}
		nonce++;

	} while (nonce < max_nonce && !(*restart));

	pdata[19] = nonce;
	*hashes_done = pdata[19] - first_nonce + 1;
	return 0;
}

void timetravel_set_target( struct work* work, double job_diff )
{
 work_set_target( work, job_diff / (256.0 * opt_diff_factor) );
}

// set_data_endian is a reasonable gate to use, it's called upon receipt
// of new work (new ntime) and has the right arg to access it.
void timetravel_calc_perm( struct work *work )
{
   // We want to permute algorithms. To get started we
   // initialize an array with a sorted sequence of unique
   // integers where every integer represents its own algorithm.
   int ntime, steps, i;
   be32enc( &ntime, work->data[ STD_NTIME_INDEX ] );
   steps = ( ntime - HASH_FUNC_BASE_TIMESTAMP )
                         % HASH_FUNC_COUNT_PERMUTATIONS;
   for ( i = 0; i < HASH_FUNC_COUNT; i++ ) 
        permutation[i] = i;
   for ( i = 0; i < steps; i++ ) 
        next_permutation( permutation, permutation + HASH_FUNC_COUNT );
}

bool register_timetravel_algo( algo_gate_t* gate )
{
  gate->optimizations = SSE2_OPT | AES_OPT | AVX_OPT | AVX2_OPT;
  init_tt_ctx();
  gate->scanhash   = (void*)&scanhash_timetravel;
  gate->hash       = (void*)&timetravel_hash;
  gate->set_target = (void*)&timetravel_set_target;
  gate->get_max64  = (void*)&get_max64_0xffffLL;
//  gate->set_work_data_endian = (void*)&timetravel_calc_perm;
  return true;
};

