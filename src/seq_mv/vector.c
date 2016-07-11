/*BHEADER**********************************************************************
 * Copyright (c) 2008,  Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory.
 * This file is part of HYPRE.  See file COPYRIGHT for details.
 *
 * HYPRE is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License (as published by the Free
 * Software Foundation) version 2.1 dated February 1999.
 *
 * $Revision$
 ***********************************************************************EHEADER*/

/******************************************************************************
 *
 * Member functions for hypre_Vector class.
 *
 *****************************************************************************/

#include "seq_mv.h"
#include <assert.h>

/*--------------------------------------------------------------------------
 * hypre_SeqVectorCreate
 *--------------------------------------------------------------------------*/

hypre_Vector *
hypre_SeqVectorCreate( HYPRE_Int size )
{
   hypre_Vector  *vector;

   vector = hypre_CTAlloc(hypre_Vector, 1);

   hypre_VectorData(vector) = NULL;
   hypre_VectorSize(vector) = size;

   hypre_VectorNumVectors(vector) = 1;
   hypre_VectorMultiVecStorageMethod(vector) = 0;

   /* set defaults */
   hypre_VectorOwnsData(vector) = 1;

#ifdef HYPRE_USE_CUDA
   hypre_VectorDevInit(vector);
#endif
   return vector;
}

/*--------------------------------------------------------------------------
 * hypre_SeqMultiVectorCreate
 *--------------------------------------------------------------------------*/

hypre_Vector *
hypre_SeqMultiVectorCreate( HYPRE_Int size, HYPRE_Int num_vectors )
{
   hypre_Vector *vector = hypre_SeqVectorCreate(size);
   hypre_VectorNumVectors(vector) = num_vectors;
   return vector;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorDestroy
 *--------------------------------------------------------------------------*/

HYPRE_Int 
hypre_SeqVectorDestroy( hypre_Vector *vector )
{
   HYPRE_Int  ierr=0;

   if (vector)
   {
#ifdef HYPRE_USE_CUDA
if (hypre_VectorDevice(vector)->mapped){
  PUSH_RANGE("VecDestroy",5)
   cudaHostUnregister(hypre_VectorData(vector));
   cudaError_t ce=cudaFree(hypre_VectorDataDevice(vector));
   if (ce!=cudaSuccess){
     printf("CUDA ERROR AT %s\n",cudaGetErrorString(ce));
   }
   gpuErrchk(cudaEventDestroy(vector->dev->event));
   hypre_TFree(hypre_VectorDevice(vector));
   POP_RANGE
   hypre_VectorDevice(vector)=NULL;
   }
 #endif
       if ( hypre_VectorOwnsData(vector) )
       {
	  hypre_TFree(hypre_VectorData(vector));
       }
       hypre_TFree(vector);
    }

    return ierr;
 }

 /*--------------------------------------------------------------------------
  * hypre_SeqVectorInitialize
  *--------------------------------------------------------------------------*/

 HYPRE_Int 
 hypre_SeqVectorInitialize( hypre_Vector *vector )
 {
    HYPRE_Int  size = hypre_VectorSize(vector);
    HYPRE_Int  ierr = 0;
    HYPRE_Int  num_vectors = hypre_VectorNumVectors(vector);
    HYPRE_Int  multivec_storage_method = hypre_VectorMultiVecStorageMethod(vector);

    if ( ! hypre_VectorData(vector) )
       hypre_VectorData(vector) = hypre_TAlloc(HYPRE_Complex, num_vectors*size);

    if ( multivec_storage_method == 0 )
    {
       hypre_VectorVectorStride(vector) = size;
       hypre_VectorIndexStride(vector) = 1;
    }
    else if ( multivec_storage_method == 1 )
    {
       hypre_VectorVectorStride(vector) = 1;
       hypre_VectorIndexStride(vector) = num_vectors;
    }
    else
       ++ierr;

#ifdef HYPRE_USE_CUDA
   hypre_VectorDevInit(vector);
#endif
    return ierr;
 }

 /*--------------------------------------------------------------------------
  * hypre_SeqVectorSetDataOwner
  *--------------------------------------------------------------------------*/

 HYPRE_Int 
 hypre_SeqVectorSetDataOwner( hypre_Vector *vector,
			      HYPRE_Int     owns_data   )
 {
    HYPRE_Int    ierr=0;

    hypre_VectorOwnsData(vector) = owns_data;

    return ierr;
 }

 /*--------------------------------------------------------------------------
  * ReadVector
  *--------------------------------------------------------------------------*/

 hypre_Vector *
 hypre_SeqVectorRead( char *file_name )
 {
    hypre_Vector  *vector;

    FILE    *fp;

    HYPRE_Complex *data;
    HYPRE_Int      size;

    HYPRE_Int      j;

    /*----------------------------------------------------------
     * Read in the data
     *----------------------------------------------------------*/

    fp = fopen(file_name, "r");

    hypre_fscanf(fp, "%d", &size);

    vector = hypre_SeqVectorCreate(size);
    hypre_SeqVectorInitialize(vector);

    data = hypre_VectorData(vector);
    for (j = 0; j < size; j++)
    {
       hypre_fscanf(fp, "%le", &data[j]);
    }

    fclose(fp);

    /* multivector code not written yet >>> */
    hypre_assert( hypre_VectorNumVectors(vector) == 1 );

    return vector;
 }

 /*--------------------------------------------------------------------------
  * hypre_SeqVectorPrint
  *--------------------------------------------------------------------------*/

 HYPRE_Int
 hypre_SeqVectorPrint( hypre_Vector *vector,
		       char         *file_name )
 {
    FILE    *fp;

    HYPRE_Complex *data;
    HYPRE_Int      size, num_vectors, vecstride, idxstride;

    HYPRE_Int      i, j;
    HYPRE_Complex  value;

    HYPRE_Int      ierr = 0;

    num_vectors = hypre_VectorNumVectors(vector);
    vecstride = hypre_VectorVectorStride(vector);
    idxstride = hypre_VectorIndexStride(vector);

    /*----------------------------------------------------------
     * Print in the data
     *----------------------------------------------------------*/

    data = hypre_VectorData(vector);
    size = hypre_VectorSize(vector);

    fp = fopen(file_name, "w");

    if ( hypre_VectorNumVectors(vector) == 1 )
    {
       hypre_fprintf(fp, "%d\n", size);
    }
    else
    {
       hypre_fprintf(fp, "%d vectors of size %d\n", num_vectors, size );
    }

    if ( num_vectors>1 )
    {
       for ( j=0; j<num_vectors; ++j )
       {
	  hypre_fprintf(fp, "vector %d\n", j );
	  for (i = 0; i < size; i++)
	  {
	     value = data[ j*vecstride + i*idxstride ];
 #ifdef HYPRE_COMPLEX
	     hypre_fprintf(fp, "%.14e , %.14e\n",
			   hypre_creal(value), hypre_cimag(value));
 #else
	     hypre_fprintf(fp, "%.14e\n", value);
 #endif
	  }
       }
    }
    else
    {
       for (i = 0; i < size; i++)
       {
 #ifdef HYPRE_COMPLEX
	  hypre_fprintf(fp, "%.14e , %.14e\n",
			hypre_creal(data[i]), hypre_cimag(data[i]));
 #else
	  hypre_fprintf(fp, "%.14e\n", data[i]);
 #endif
       }
    }

    fclose(fp);

    return ierr;
 }

 /*--------------------------------------------------------------------------
  * hypre_SeqVectorSetConstantValues
  *--------------------------------------------------------------------------*/

 HYPRE_Int
 hypre_SeqVectorSetConstantValues( hypre_Vector *v,
				   HYPRE_Complex value )
 {
 #ifdef HYPRE_PROFILE
    hypre_profile_times[HYPRE_TIMER_ID_BLAS1] -= hypre_MPI_Wtime();
 #endif

    HYPRE_Complex *vector_data = hypre_VectorData(v);
    HYPRE_Int      size        = hypre_VectorSize(v);

    HYPRE_Int      i;

    HYPRE_Int      ierr  = 0;

    size *=hypre_VectorNumVectors(v);

 #ifdef HYPRE_USING_OPENMP
 #pragma omp parallel for private(i) HYPRE_SMP_SCHEDULE
 #endif
    for (i = 0; i < size; i++)
       vector_data[i] = value;

#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] += hypre_MPI_Wtime();
#endif

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorSetRandomValues
 *
 *     returns vector of values randomly distributed between -1.0 and +1.0
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_SeqVectorSetRandomValues( hypre_Vector *v,
                                HYPRE_Int           seed )
{
   HYPRE_Complex *vector_data = hypre_VectorData(v);
   HYPRE_Int      size        = hypre_VectorSize(v);
           
   HYPRE_Int      i;
           
   HYPRE_Int      ierr  = 0;
   hypre_SeedRand(seed);

   size *=hypre_VectorNumVectors(v);

/* RDF: threading this loop may cause problems because of hypre_Rand() */
   for (i = 0; i < size; i++)
      vector_data[i] = 2.0 * hypre_Rand() - 1.0;

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorCopy
 * copies data from x to y
 * if size of x is larger than y only the first size_y elements of x are 
 * copied to y
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_SeqVectorCopy( hypre_Vector *x,
                     hypre_Vector *y )
{
#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] -= hypre_MPI_Wtime();
#endif

   HYPRE_Complex *x_data = hypre_VectorData(x);
   HYPRE_Complex *y_data = hypre_VectorData(y);
   HYPRE_Int      size   = hypre_VectorSize(x);
   HYPRE_Int      size_y   = hypre_VectorSize(y);
           
   HYPRE_Int      i;
           
   HYPRE_Int      ierr = 0;

   if (size > size_y) size = size_y;
   size *=hypre_VectorNumVectors(x);
#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(i) HYPRE_SMP_SCHEDULE
#endif
      for (i = 0; i < size; i++)
         y_data[i] = x_data[i];
   //memcpy(y_data,x_data,size*sizeof(HYPRE_Complex));

#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] += hypre_MPI_Wtime();
#endif

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorCloneDeep
 * Returns a complete copy of x - a deep copy, with its own copy of the data.
 *--------------------------------------------------------------------------*/

hypre_Vector *
hypre_SeqVectorCloneDeep( hypre_Vector *x )
{
   HYPRE_Int      size   = hypre_VectorSize(x);
   HYPRE_Int      num_vectors   = hypre_VectorNumVectors(x);
   hypre_Vector * y = hypre_SeqMultiVectorCreate( size, num_vectors );

   hypre_VectorMultiVecStorageMethod(y) = hypre_VectorMultiVecStorageMethod(x);
   hypre_VectorVectorStride(y) = hypre_VectorVectorStride(x);
   hypre_VectorIndexStride(y) = hypre_VectorIndexStride(x);

   hypre_SeqVectorInitialize(y);
   hypre_SeqVectorCopy( x, y );

   return y;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorCloneShallow
 * Returns a complete copy of x - a shallow copy, pointing the data of x
 *--------------------------------------------------------------------------*/

hypre_Vector *
hypre_SeqVectorCloneShallow( hypre_Vector *x )
{
   HYPRE_Int      size   = hypre_VectorSize(x);
   HYPRE_Int      num_vectors   = hypre_VectorNumVectors(x);
   hypre_Vector * y = hypre_SeqMultiVectorCreate( size, num_vectors );

   hypre_VectorMultiVecStorageMethod(y) = hypre_VectorMultiVecStorageMethod(x);
   hypre_VectorVectorStride(y) = hypre_VectorVectorStride(x);
   hypre_VectorIndexStride(y) = hypre_VectorIndexStride(x);

   hypre_VectorData(y) = hypre_VectorData(x);
   hypre_SeqVectorSetDataOwner( y, 0 );
   hypre_SeqVectorInitialize(y);

   return y;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorScale
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_SeqVectorScale( HYPRE_Complex alpha,
                      hypre_Vector *y     )
{
#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] -= hypre_MPI_Wtime();
#endif

   HYPRE_Complex *y_data = hypre_VectorData(y);
   HYPRE_Int      size   = hypre_VectorSize(y);
           
   HYPRE_Int      i;
           
   HYPRE_Int      ierr = 0;

   size *=hypre_VectorNumVectors(y);

#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(i) HYPRE_SMP_SCHEDULE
#endif
   for (i = 0; i < size; i++)
      y_data[i] *= alpha;

#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] += hypre_MPI_Wtime();
#endif

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorAxpy
 *--------------------------------------------------------------------------*/

HYPRE_Int
hypre_SeqVectorAxpy( HYPRE_Complex alpha,
                     hypre_Vector *x,
                     hypre_Vector *y     )
{
#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] -= hypre_MPI_Wtime();
#endif

   HYPRE_Complex *x_data = hypre_VectorData(x);
   HYPRE_Complex *y_data = hypre_VectorData(y);
   HYPRE_Int      size   = hypre_VectorSize(x);
           
   HYPRE_Int      i;
           
   HYPRE_Int      ierr = 0;

   size *=hypre_VectorNumVectors(x);

#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(i) HYPRE_SMP_SCHEDULE
#endif
   for (i = 0; i < size; i++)
      y_data[i] += alpha * x_data[i];

#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] += hypre_MPI_Wtime();
#endif

   return ierr;
}

/*--------------------------------------------------------------------------
 * hypre_SeqVectorInnerProd
 *--------------------------------------------------------------------------*/

HYPRE_Real   hypre_SeqVectorInnerProd( hypre_Vector *x,
                                       hypre_Vector *y )
{
#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] -= hypre_MPI_Wtime();
#endif

   HYPRE_Complex *x_data = hypre_VectorData(x);
   HYPRE_Complex *y_data = hypre_VectorData(y);
   HYPRE_Int      size   = hypre_VectorSize(x);
           
   HYPRE_Int      i;

   HYPRE_Real     result = 0.0;

   size *=hypre_VectorNumVectors(x);

#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(i) reduction(+:result) HYPRE_SMP_SCHEDULE
#endif
   for (i = 0; i < size; i++)
      result += hypre_conj(y_data[i]) * x_data[i];

#ifdef HYPRE_PROFILE
   hypre_profile_times[HYPRE_TIMER_ID_BLAS1] += hypre_MPI_Wtime();
#endif

   return result;
}

/*--------------------------------------------------------------------------
 * hypre_VectorSumElts:
 * Returns the sum of all vector elements.
 *--------------------------------------------------------------------------*/

HYPRE_Complex hypre_VectorSumElts( hypre_Vector *vector )
{
   HYPRE_Complex  sum = 0;
   HYPRE_Complex *data = hypre_VectorData( vector );
   HYPRE_Int      size = hypre_VectorSize( vector );
   HYPRE_Int      i;

#ifdef HYPRE_USING_OPENMP
#pragma omp parallel for private(i) reduction(+:sum) HYPRE_SMP_SCHEDULE
#endif
   for ( i=0; i<size; ++i ) sum += data[i];

   return sum;
}
#ifdef HYPRE_USE_CUDA
void hypre_VectorMapToDevice(hypre_Vector *vector){
  if (vector->num_vectors!=1){
    fprintf(stderr,"ERROR:: CUDA routines cannot handle mult-vectors\n");
    exit(2);
  }
  if (!hypre_VectorDevice(vector)){
    hypre_VectorDevice(vector)=hypre_CTAlloc(cuda_Vector, 1);
    hypre_VectorDataDevice(vector)=NULL;
    vector->dev->offset1=-1;
    vector->dev->offset2=-1;
    vector->dev->send_to_device=1; // Default set to copy to device
    vector->dev->bring_from_device=1;  // Default set to copy from device
    vector->dev->nosync=0; // Default is to sync
    //printf("Vector mapped %p\n",hypre_VectorDevice(vector));
  } else {
    //printf("Call to map with valid device pointer %p\n",hypre_VectorDevice(vector));
  }
  if (!hypre_VectorDataDevice(vector)){
    hypre_VectorDevice(vector)->mapped=1;
    gpuErrchk(cudaEventCreateWithFlags(&(vector->dev->event),cudaEventDisableTiming));
    //gpuErrchk(cudaEventCreate(&(vector->dev->event)));
    gpuErrchk(cudaStreamCreate(&vector->dev->s0));
    gpuErrchk(cudaStreamCreate(&vector->dev->s1));
    size_t size=hypre_VectorSize(vector)*sizeof(HYPRE_Complex);
    size_t pgz=getpagesize();
    size=((size+pgz-1)/pgz)*pgz;
    gpuErrchk(cudaMalloc((void**)&hypre_VectorDataDevice(vector),size));
    cudaError_t code = cudaHostRegister(hypre_VectorData(vector),size,cudaHostRegisterDefault);
    //cudaError_t code=cudaSuccess;
    if (code != cudaSuccess) 
       {
	 printf("Failed to register pointer %p of size %d CODE %d\n",hypre_VectorData(vector),size,code);
	 //printf("Register fail: %s size = %d\n", cudaGetErrorString(code), size);
       }
  }
}
void hypre_VectorDevInit(hypre_Vector *vector){
  if (!hypre_VectorDevice(vector)){
    hypre_VectorDevice(vector)=hypre_CTAlloc(cuda_Vector, 1);
    hypre_VectorDataDevice(vector)=NULL;
    vector->dev->offset1=-1;
    vector->dev->offset2=-1;
    vector->dev->send_to_device=1; // Default set to copy to device
    vector->dev->bring_from_device=1;  // Default set to copy from device
    vector->dev->nosync=0; // Default is to sync
    vector->dev->mapped=0; 
    vector->dev->s0=0;
    vector->dev->s1=0;
    vector->dev->fraction=1.0; // Default 70% in favor of the device
    vector->dev->cycle=0;
    //printf("Vector mapped %p\n",hypre_VectorDevice(vector));
  } 
}
// Synchronous vector copy

HYPRE_Int hypre_VectorH2D(hypre_Vector *vector){
  PUSH_RANGE("VecDataSend",0);
  gpuErrchk(cudaMemcpy(hypre_VectorDataDevice(vector),hypre_VectorData(vector),
		       (size_t)(vector->size*sizeof(HYPRE_Complex)), 
		       cudaMemcpyHostToDevice));
  POP_RANGE;
  
}

void hypre_VectorD2H(hypre_Vector *vector){
  PUSH_RANGE("VecDataRecv",1);
  gpuErrchk(cudaMemcpy(hypre_VectorData(vector),hypre_VectorDataDevice(vector),
		   (size_t)(vector->size*sizeof(HYPRE_Complex)), 
			cudaMemcpyDeviceToHost));
  POP_RANGE;
}

void hypre_VectorD2HCross(hypre_Vector *dest, hypre_Vector *src,int offset, int size){
  PUSH_RANGE("VecDataXRecv",2);
  gpuErrchk(cudaMemcpy(hypre_VectorData(dest)+offset,hypre_VectorDataDevice(src)+offset,
		       (size_t)(size*sizeof(HYPRE_Complex)), 
			cudaMemcpyDeviceToHost));
  POP_RANGE;
}

// Asynchronous vector copy

HYPRE_Int hypre_VectorH2DAsync(hypre_Vector *vector,cudaStream_t s){
  PUSH_RANGE("VecDataSendAsync",0);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorDataDevice(vector),hypre_VectorData(vector),
		       (size_t)(vector->size*sizeof(HYPRE_Complex)), 
			    cudaMemcpyHostToDevice,s));
  POP_RANGE;
  
}
HYPRE_Int hypre_VectorH2DAsyncPartial(hypre_Vector *vector,size_t size,cudaStream_t s){
  PUSH_RANGE("VecDataSendAsyncPartial",0);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorDataDevice(vector),hypre_VectorData(vector),
		       (size_t)(size*sizeof(HYPRE_Complex)), 
			    cudaMemcpyHostToDevice,s));
  POP_RANGE;
  
}
void hypre_VectorD2HAsync(hypre_Vector *vector,cudaStream_t s){
  PUSH_RANGE("VecDataRecvAsync",1);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorData(vector),hypre_VectorDataDevice(vector),
		   (size_t)(vector->size*sizeof(HYPRE_Complex)), 
			    cudaMemcpyDeviceToHost,s));
  POP_RANGE;
}
void hypre_VectorD2HAsyncPartial(hypre_Vector *vector,size_t size,cudaStream_t s){
  PUSH_RANGE("VecDataRecvAsyncPartial",1);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorData(vector),hypre_VectorDataDevice(vector),
		   (size_t)(size*sizeof(HYPRE_Complex)), 
			    cudaMemcpyDeviceToHost,s));
  POP_RANGE;
}
void hypre_VectorH2DCrossAsync(hypre_Vector *dest, hypre_Vector *src,int offset, int size,cudaStream_t s){
  PUSH_RANGE("VecDataXSend",2);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorDataDevice(dest)+offset,hypre_VectorData(src)+offset,
		       (size_t)(size*sizeof(HYPRE_Complex)), 
		       cudaMemcpyHostToDevice,s));
  POP_RANGE;
}
void hypre_VectorD2HCrossAsync(hypre_Vector *dest, hypre_Vector *src,int offset, int size,cudaStream_t s){
  PUSH_RANGE("VecDataXRecv",2);
  gpuErrchk(cudaMemcpyAsync(hypre_VectorData(dest)+offset,hypre_VectorDataDevice(src)+offset,
		       (size_t)(size*sizeof(HYPRE_Complex)), 
		       cudaMemcpyDeviceToHost,s));
  POP_RANGE;
}

#endif
