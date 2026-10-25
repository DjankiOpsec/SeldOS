/*
 * SeldOS - Humboldt Kernel Project
 * NanoJPEG Header (userspace/bin/cctv/nanojpeg.h)
 * Lightweight Freestanding Baseline JPEG Decoder
 * KeyJ's Research License / GNU AGPLv3 Compatible
 */

#ifndef _NANOJPEG_H_
#define _NANOJPEG_H_

#include "stdlib.h"
#include "string.h"

typedef enum _nj_result {
    NJ_OK = 0,        /* no error, decoding successful */
    NJ_NO_JPEG,       /* not a JPEG file */
    NJ_UNSUPPORTED,   /* unsupported format */
    NJ_OUT_OF_MEM,    /* out of memory */
    NJ_INTERNAL_ERR,  /* internal error */
    NJ_SYNTAX_ERROR,  /* syntax error */
    __NJ_FINISHED,    /* used internally, will never be reported */
} nj_result_t;

typedef struct _nj_ctx nj_context_t;

nj_context_t*  njInit(void);
void           njDone(nj_context_t* nj);
nj_result_t    njDecode(nj_context_t* nj, const void* jpeg, const int size);
int            njGetWidth(nj_context_t* nj);
int            njGetHeight(nj_context_t* nj);
int            njIsColor(nj_context_t* nj);
unsigned char* njGetImage(nj_context_t* nj);
int            njGetImageSize(nj_context_t* nj);
void           njFreeImage(unsigned char* image);

#endif /* _NANOJPEG_H_ */
