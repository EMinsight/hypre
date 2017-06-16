#include <stdio.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include "_hypre_utilities.h"
#define gpuErrchk2(ans) { gpuAssert2((ans), __FILE__, __LINE__); }
inline void gpuAssert2(cudaError_t code, const char *file, hypre_int line)
{
   if (code != cudaSuccess) 
   {
     printf("GPUassert2: %s %s %d\n", cudaGetErrorString(code), file, line);
     exit(2);
   }
}



extern "C"{
  __global__
  void VecScaleKernelText(HYPRE_Complex *__restrict__ u, const HYPRE_Complex *__restrict__ v, const HYPRE_Complex *__restrict__ l1_norm, hypre_int num_rows){
    hypre_int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i<num_rows){
      u[i]+=__ldg(v+i)/__ldg(l1_norm+i);
    }
  }
}

extern "C"{
  __global__
  void VecScaleKernel(HYPRE_Complex *__restrict__ u, const HYPRE_Complex *__restrict__ v, const HYPRE_Complex * __restrict__ l1_norm, hypre_int num_rows){
    hypre_int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i<num_rows){
      u[i]+=v[i]/l1_norm[i];
  }
  }
}

extern "C"{
  void VecScale(HYPRE_Complex *u, HYPRE_Complex *v, HYPRE_Complex *l1_norm, hypre_int num_rows,cudaStream_t s){
    PUSH_RANGE_PAYLOAD("VECSCALE",1,num_rows);
    //fprintf(stderr,"VECSCALE\n");
    const hypre_int tpb=64;
    hypre_int num_blocks=num_rows/tpb+1;
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    //ptrChk(u);
    //ptrChk(v);
    //ptrChk(l1_norm);
    MemPrefetchSized(l1_norm,num_rows*sizeof(HYPRE_Complex),HYPRE_DEVICE,s);
    VecScaleKernel<<<num_blocks,tpb,0,s>>>(u,v,l1_norm,num_rows);
#ifdef CATCH_LAUNCH_ERRORS    
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    gpuErrchk2(cudaStreamSynchronize(s));
    POP_RANGE;
  }
}


extern "C"{

  __global__
  void VecCopyKernel(HYPRE_Complex* __restrict__ tgt, const HYPRE_Complex* __restrict__ src, hypre_int size){
    hypre_int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i<size) tgt[i]=src[i];
}
  void VecCopy(HYPRE_Complex* tgt, const HYPRE_Complex* src, hypre_int size,cudaStream_t s){
    hypre_int tpb=64;
    hypre_int num_blocks=size/tpb+1;
    //ptrChk((void*)tgt);
    //ptrChk((void*)src);
    PUSH_RANGE_PAYLOAD("VecCopy",5,size);
    //fprintf(stderr,"VecCopy %d %d\n",size,num_blocks);
    //MemPrefetch(tgt,0,s);
    //MemPrefetch(src,0,s);
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    VecCopyKernel<<<num_blocks,tpb,0,s>>>(tgt,src,size);
#ifdef CATCH_LAUNCH_ERRORS    
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    //gpuErrchk2(cudaStreamSynchronize(s));
    POP_RANGE;
  }
}
extern "C"{

  __global__
  void VecSetKernel(HYPRE_Complex* __restrict__ tgt, const HYPRE_Complex value,hypre_int size){
    hypre_int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i<size) tgt[i]=value;
}
  void VecSet(HYPRE_Complex* tgt, hypre_int size, HYPRE_Complex value, cudaStream_t s){
    hypre_int tpb=64;
    //cudaDeviceSynchronize();
    //fprintf(stderr,"VECSET\n");
    MemPrefetchSized(tgt,size*sizeof(HYPRE_Complex),HYPRE_DEVICE,s);
    hypre_int num_blocks=size/tpb+1;
    VecSetKernel<<<num_blocks,tpb,0,s>>>(tgt,value,size);
    cudaStreamSynchronize(s);
    //cudaDeviceSynchronize();
  }
}
extern "C"{
  __global__
  void  PackOnDeviceKernel(HYPRE_Complex* __restrict__ send_data,const HYPRE_Complex* __restrict__ x_local_data, const hypre_int* __restrict__ send_map, hypre_int begin,hypre_int end){
    hypre_int i = begin+blockIdx.x * blockDim.x + threadIdx.x;
    if (i<end){
      send_data[i-begin]=x_local_data[send_map[i]];
    }
  }
  void PackOnDevice(HYPRE_Complex *send_data,HYPRE_Complex *x_local_data, hypre_int *send_map, hypre_int begin,hypre_int end,cudaStream_t s){
    if ((end-begin)<=0) return;
    //printf("PACK ON DEVICE \n");
    //ptrChk(send_data);
    //ptrChk(x_local_data);
    //ptrChk(send_map);
    
    hypre_int tpb=64;
    hypre_int num_blocks=(end-begin)/tpb+1;
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    PackOnDeviceKernel<<<num_blocks,tpb,0,s>>>(send_data,x_local_data,send_map,begin,end);
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    PUSH_RANGE("PACK_PREFETCH",1);
#ifndef HYPRE_GPU_USE_PINNED
    MemPrefetchSized((void*)send_data,(end-begin)*sizeof(HYPRE_Complex),cudaCpuDeviceId,s);
#endif
    POP_RANGE;
    //gpuErrchk2(cudaStreamSynchronize(s));
  }
}
  
  // Scale vector by scalar

extern "C"{
__global__
void VecScaleScalarKernel(HYPRE_Complex *__restrict__ u, const HYPRE_Complex alpha ,hypre_int num_rows){
  hypre_int i = blockIdx.x * blockDim.x + threadIdx.x;
  //if (i<5) printf("DEVICE %d %lf %lf %lf\n",i,u[i],v[i],l1_norm[i]);
  if (i<num_rows){
    u[i]*=alpha;
    //if (i==0) printf("Diff Device %d %lf %lf %lf\n",i,u[i],v[i],l1_norm[i]);
  }
}
}
extern "C"{
  hypre_int VecScaleScalar(HYPRE_Complex *u, const HYPRE_Complex alpha,  hypre_int num_rows,cudaStream_t s){
    PUSH_RANGE("SEQVECSCALE",4);
    hypre_int num_blocks=num_rows/64+1;
    
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    VecScaleScalarKernel<<<num_blocks,64,0,s>>>(u,alpha,num_rows);
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif
    gpuErrchk2(cudaStreamSynchronize(s));
    POP_RANGE;
    return 0;
  }
}


extern "C"{
  __global__ 
void CSRDiagScaleKernel(HYPRE_Real *x_data, HYPRE_Real *y_data, HYPRE_Real *A_data, hypre_int *A_i, hypre_int local_size)
  {
    hypre_int i= blockIdx.x * blockDim.x + threadIdx.x;
    if (i<local_size){
      x_data[i] = y_data[i]/A_data[A_i[i]];
    }
  }
  void CSRDiagScale(HYPRE_Real *x_data, HYPRE_Real *y_data, HYPRE_Real *A_data, hypre_int *A_i, hypre_int local_size){
    hypre_int num_threads=1024;
    hypre_int num_blocks=local_size/num_threads+1;
    ptrChk(x_data);
    ptrChk(y_data);
    ptrChk(A_data);
    ptrChk(A_i);
    fprintf(stderr,"MIGHT NEED PREFETCHING\n");
    CSRDiagScaleKernel<<<num_blocks,num_threads,0,HYPRE_STREAM(4)>>>(x_data,y_data,A_data,A_i,local_size);
    gpuErrchk2(cudaStreamSynchronize(HYPRE_STREAM(4)));
  }
}





extern "C"{
__global__
void SpMVCudaKernel(HYPRE_Complex* __restrict__ y,HYPRE_Complex alpha, const HYPRE_Complex* __restrict__ A_data, const hypre_int* __restrict__ A_i, const hypre_int* __restrict__ A_j, const HYPRE_Complex* __restrict__ x, HYPRE_Complex beta, hypre_int num_rows)
{
  hypre_int i= blockIdx.x * blockDim.x + threadIdx.x;
  if (i<num_rows){
    HYPRE_Complex temp = 0.0;
    hypre_int jj;
    for (jj = A_i[i]; jj < A_i[i+1]; jj++){
      hypre_int ajj=A_j[jj];
      temp += A_data[jj] * x[ajj];
    }
    y[i] =y[i]*beta+alpha*temp;
  }
}

__global__
void SpMVCudaKernelZB(HYPRE_Complex* __restrict__ y,HYPRE_Complex alpha, const HYPRE_Complex* __restrict__ A_data, const hypre_int* __restrict__ A_i, const hypre_int* __restrict__ A_j, const HYPRE_Complex* __restrict__ x, hypre_int num_rows)
{
  hypre_int i= blockIdx.x * blockDim.x + threadIdx.x;
  if (i<num_rows){
    HYPRE_Complex temp = 0.0;
    hypre_int jj;
    for (jj = A_i[i]; jj < A_i[i+1]; jj++){
      hypre_int ajj=A_j[jj];
      temp += A_data[jj] * x[ajj];
    }
    y[i] = alpha*temp;
  }
}
  void SpMVCuda(hypre_int num_rows,HYPRE_Complex alpha, HYPRE_Complex *A_data,hypre_int *A_i, hypre_int *A_j, HYPRE_Complex *x, HYPRE_Complex beta, HYPRE_Complex *y){
    hypre_int num_threads=64;
    hypre_int num_blocks=num_rows/num_threads+1;
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif    
    if (beta==0.0)
      SpMVCudaKernelZB<<<num_blocks,num_threads>>>(y,alpha,A_data,A_i,A_j,x,num_rows);
    else
      SpMVCudaKernel<<<num_blocks,num_threads>>>(y,alpha,A_data,A_i,A_j,x,beta,num_rows);
#ifdef CATCH_LAUNCH_ERRORS
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
#endif

}
}
extern "C"{
  __global__
  void CompileFlagSafetyCheck(hypre_int actual){
#ifdef __CUDA_ARCH__
    hypre_int cudarch=__CUDA_ARCH__;
    if (cudarch!=actual){
      printf("WARNING :: nvcc -arch flag does not match actual device architecture\nWARNING :: The code can fail silently and produce wrong results\n");
      printf("Arch specified at compile = sm_%d Actual device = sm_%d\n",cudarch/10,actual/10);
    } 
#else
    printf("ERROR:: CUDA_ ARCH is not defined \n This should not be happening\n");
#endif
  }
}
extern "C"{
  void CudaCompileFlagCheck(){
    hypre_int devCount;
    cudaGetDeviceCount(&devCount);
    hypre_int i;
    hypre_int cudarch_actual;
    for(i = 0; i < devCount; ++i)
      {
	struct cudaDeviceProp props;
	cudaGetDeviceProperties(&props, i);
	cudarch_actual=props.major*100+props.minor*10;
    }
    gpuErrchk2(cudaPeekAtLastError());
    gpuErrchk2(cudaDeviceSynchronize());
    CompileFlagSafetyCheck<<<1,1,0,0>>>(cudarch_actual);
    cudaError_t code=cudaPeekAtLastError();
    if (code != cudaSuccess)
      {
	fprintf(stderr,"ERROR in CudaCompileFlagCheck%s \n", cudaGetErrorString(code));
	fprintf(stderr,"ERROR :: Check if compile arch flags match actual device arch = sm_%d\n",cudarch_actual/10);
	exit(2);
      }
    gpuErrchk2(cudaDeviceSynchronize());
  }
}