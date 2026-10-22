/*
 * XOMP compatibility layer used by OpenMP lowering.
 * Runtime backend in REX is LLVM OpenMP (libomp/__kmpc).
 */
#ifndef LIB_XOMP_H
#define LIB_XOMP_H

// Fortran outlined function uses one parameter for each variable to be passed
// by reference We predefine a max number of parameters to be allowed here.
#define MAX_OUTLINED_FUNC_PARAMETER_COUNT 256
#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>

#include <stdbool.h>

#include <stdio.h>

#include <stdlib.h> // for abort()
#include <sys/time.h>

extern void xomp_acc_init(void);

extern void XOMP_task(void (*)(void *), void *, void (*)(void *, void *), long,
                      long, bool, unsigned);
extern void XOMP_taskwait(void);

// Scheduler helper entry points.

// simplest static even divide a range start:size into several chunks of
// offset:size pair This can be used to divide up loop or data (array range).
// original range is start_offset:orig_size
// chunk_id starts from 0
// output is the offset and size for the current chunk id
extern void XOMP_static_even_divide(long start, long orig_size, int chunk_count,
                                    int chunk_id, long *chunk_offset,
                                    long *chunk_size);

extern void XOMP_barrier(void);

//--------------------- extensions to support OpenMP accelerator model
// experimental implementation------
// We only include

// Set the device id to be used by the current task
extern void xomp_set_default_device(int devID);
//--------------------- kernel launch ------------------

// the max number of threads per thread block of the first available device
extern size_t xomp_get_maxThreadsPerBlock(int devID);

// get the max number of 1D blocks for a given input length
extern size_t xomp_get_max1DBlock(int devID, size_t ss);

// Get the max number threads for one dimension (x or y) of a 2D block
// Two factors are considered: the total number of threads within the 2D block
// must<= total threads per block
//  x * y <= maxThreadsPerBlock 512 or 1024
// each dimension: the number of threads must <= maximum x/y-dimension
//    x <= maxThreadsDim[0],  1024
//    y <= maxThreadsDim[1], 1024
//  maxThreadsDim[0] happens to be equal to  maxThreadsDim[1] so we use a single
//  function to calculate max segments for both dimensions
extern size_t xomp_get_max_threads_per_dimesion_2D(int devID);

// return the max number of segments for a dimension (either x or y) of a 2D
// block
extern size_t xomp_get_maxSegmentsPerDimensionOf2DBlock(int devID,
                                                        size_t dimension_size);

//------------------memory
// allocation/copy/free---------------------------------- Allocate device memory
// and return the pointer
// This should be a better interface than cudaMalloc()
// since it mimics malloc() closely
/*
return a pointer to the allocated space
   * upon successful completion with size not equal to 0
return a null pointer if
  * size is 0
  * failure due to any reason
*/
extern void *xomp_deviceMalloc(size_t size);

// A host version
extern void *xomp_hostMalloc(size_t size);

// memory copy from src to dest, return the pointer to dest. NULL pointer if
// anything is wrong
extern void *xomp_memcpyHostToDevice(void *dest, const void *src, size_t n_n);
extern void *xomp_memcpyDeviceToHost(void *dest, const void *src, size_t n_n);
// copy a dynamically allocated host source array to linear dest address on a
// GPU device. the dimension information of the source array is given by: int
// dimensions[dimension_size], with known element size. bytes_copied reports the
// total bytes copied by this function. Note: It cannot be used copy static
// arrays declared like type array[N][M] !!
extern void *xomp_memcpyDynamicHostToDevice(void *dest, const void *src,
                                            int *dimensions,
                                            size_t dimension_size,
                                            size_t element_size,
                                            size_t *bytes_copied);

// copy linear src memory to dynamically allocated destination, with dimension
// information given by int dimensions[dimension_size] the source memory has
// total n continuous memory, with known size for each element the total bytes
// copied by this function is reported by bytes_copied
extern void *xomp_memcpyDynamicDeviceToHost(void *dest, int *dimensions,
                                            size_t dimension_size,
                                            const void *src,
                                            size_t element_size,
                                            size_t *bytes_copied);

extern void *xomp_memcpyDeviceToDevice(void *dest, const void *src, size_t n_n);
extern void *xomp_memcpyHostToHost(void *dest, const void *src,
                                   size_t n_n); // same as memcpy??

// free the device memory pointed by a pointer, return false in case of failure,
// otherwise return true
extern bool xomp_freeDevice(void *devPtr);
// free the host memory pointed by a pointer, return false in case of failure,
// otherwise return true
extern bool xomp_freeHost(void *hostPtr);

/* Allocation/Free functions for Host */
/* Allocate a multi-dimensional array
 *
 * Input parameters:
 *  int *dimensions:  an integer array storing the size of each dimension
 *  size_t dimension_num: the number of dimensions
 *  size_t esize: the size of an array element
 *
 * return:
 *  the pointer to the allocated array
 * */
extern void *xomp_mallocArray(int *dimensions, size_t dimension_num,
                              size_t esize);

extern void xomp_freeArrayPointer(void *array, int *dimensions,
                                  size_t dimension_num);

/* CUDA reduction support */
//------------ types for CUDA reduction support---------
// Reduction for regular OpenMP is supported by compiler translation. No runtime
// support is needed. For the accelerator model experimental implementation, we
// use a two-level reduction method: thread-block level within GPU +
// beyond-block level on CPU

/* an internal union type to be flexible for all types associated with reduction
   operations We don't really want to expose this to the compiler to simplify
   the compiler translation.
*/
// We try to limit the numbers of runtime data types exposed to a compiler.
// A set of integers to represent reduction operations
#define XOMP_REDUCTION_PLUS 6
#define XOMP_REDUCTION_MINUS 7
#define XOMP_REDUCTION_MUL 8
#define XOMP_REDUCTION_BITAND 9  // &
#define XOMP_REDUCTION_BITOR 10  // |
#define XOMP_REDUCTION_BITXOR 11 // ^
#define XOMP_REDUCTION_LOGAND 12 // &&
#define XOMP_REDUCTION_LOGOR 13  // ||

#define XOMP_BEYOND_BLOCK_REDUCTION_DECL(dtype)                                \
  dtype xomp_beyond_block_reduction_##dtype(dtype *per_block_results,          \
                                            int numBlocks, int reduction_op);

XOMP_BEYOND_BLOCK_REDUCTION_DECL(int)
XOMP_BEYOND_BLOCK_REDUCTION_DECL(float)
XOMP_BEYOND_BLOCK_REDUCTION_DECL(double)

#undef XOMP_BEYOND_BLOCK_REDUCTION_DECL
#define XOMP_MAX_MAPPED_VARS                                                   \
  256 // for simplicity, we use preallocated memory for storing the mapped
      // variable list
/* Test runtime support for nested device data environments */
/* Liao, May 2, 2013*/
/* A data structure to keep track of a mapped variable
 *  Right now we use memory address of the original variable and the size of the
 * variable
 * */
struct XOMP_mapped_variable {
  void *address; // original variable's address
  // TODO: support array sections
  int *size;
  int *offset;
  int *DimSize;
  int nDim;
  int typeSize;
  void *dev_address; // the corresponding device variable's address
  bool copyTo;       // if this variable should be copied to the device first
  bool copyFrom; // if this variable should be copied back to HOST when existing
                 // the data environment
};

//! A helper function to copy a mapped variable from src to desc
extern void copy_mapped_variable(struct XOMP_mapped_variable *desc,
                                 struct XOMP_mapped_variable *src);

/* A doubly linked list for tracking Device Data Environment (DDE) */
typedef struct DDE_data {
  // Do we need this at all?  we can allocate/deallocate data without saving
  // region ID
  int Region_ID; // hash of the AST node? or just memory address of the AST node
                 // for now

  // Store the device ID in DDE
  int devID;

  // array of the newly mapped variables
  int new_variable_count;
  struct XOMP_mapped_variable *new_variables;
  // struct XOMP_mapped_variable new_variables[XOMP_MAX_MAPPED_VARS];

  // array of inherited mapped variable from possible upper level DDEs
  int inherited_variable_count;
  struct XOMP_mapped_variable *inherited_variables;
  // struct XOMP_mapped_variable  inherited_variables[XOMP_MAX_MAPPED_VARS];

  // link to its parent node
  struct DDE_data *parent;
  // link to its child node
  struct DDE_data *child;
} DDE;

// Internal control variables for target devices
extern int xomp_get_num_devices();
extern int xomp_get_max_devices(void);
extern int xomp_num_devices;
extern int xomp_max_num_devices;

// The head of the list of DDE data nodes
extern DDE **DDE_head; // TODO. We don't really need this head pointer, it is
                       // like a stack, access the end is enough
// The tail of the list
extern DDE **DDE_tail;

extern void **xomp_cuda_prop;
// create a new DDE-data node and append it to the end of the tracking list
// copy all variables from its parent node to be into the set of inherited
// variable set.
// void XOMP_Device_Data_Environment_Enter();
extern void xomp_deviceDataEnvironmentEnter(int devID);

// A all-in-one wrapper to integrate three things: 1) get inherited variable 2)
// allocate if not found, 3) register, and 4) copy into GPU operations into one
// function
//
// Based on the CPU variable address and size, also indicate if copyin or
// copyback is needed. The function will first try to inherit/reuse the same
// variable from the parent DDE. i If not successful , it will allocate a new
// data on device, register it to the current DDE, and copy CPU values when
// needed. The allocated or found device variable address will be returned.
extern void *xomp_deviceDataEnvironmentPrepareVariable(
    int devID, void *original_variable_address, int nDim, int typeSize,
    int *size, int *offset, int *vDimSize, bool copyTo, bool copyFrom);

// Check if an original  variable is already mapped in enclosing data
// environment, return its device variable's address if yes. return NULL if not
// void* XOMP_Device_Data_Environment_Get_Inherited_Variable (void*
// original_variable_address, int size);
extern void *xomp_deviceDataEnvironmentGetInheritedVariable(
    int devID, void *original_variable_address, int typeSize, int *size);

//! Add a newly mapped variable into the current DDE's new variable list
// void XOMP_Device_Data_Environment_Add_Variable (void* var_addr, int var_size,
// void * dev_addr);
extern void xomp_deviceDataEnvironmentAddVariable(
    int devID, void *var_addr, int *var_size, int *var_offset, int *var_dim,
    int nDim, int typeSize, void *dev_addr, bool copyTo, bool copyFrom);

// Exit current DDE: deallocate device memory, delete the DDE-data node from the
// end of the tracking list
// void XOMP_Device_Data_Environment_Exit();
extern void xomp_deviceDataEnvironmentExit(int devID);

#ifdef __cplusplus
}
#endif

#endif /* LIB_XOMP_H */
