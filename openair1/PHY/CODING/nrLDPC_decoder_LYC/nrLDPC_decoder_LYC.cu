 /*! \file PHY/CODING/nrLDPC_decoder_LYC/nrLDPC_decoder_LYC.cu
 * \brief LDPC cuda support BG1 all length
 * \author NCTU OpinConnect Terng-Yin Hsu,WEI-YING,LIN
 * \email tyhsu@cs.nctu.edu.tw
 * \date 13-05-2020
 * \version 
 * \note
 * \warning
 */

#include <stdio.h>
#include <unistd.h>
#include <cuda_runtime.h>
#include <cuda.h>
#include "PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"
#include "PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"

#include "bgs/BG1_I0"
#include "bgs/BG1_I1"
#include "bgs/BG1_I2"
#include "bgs/BG1_I3"
#include "bgs/BG1_I4"
#include "bgs/BG1_I5"
#include "bgs/BG1_I6"
#include "bgs/BG1_I7"
#include "bgs/BG2_I0"
#include "bgs/BG2_I1"
#include "bgs/BG2_I2"
#include "bgs/BG2_I3"
#include "bgs/BG2_I4"
#include "bgs/BG2_I5"
#include "bgs/BG2_I6"
#include "bgs/BG2_I7"

#define MAX_ITERATION 5
#define MC	1

#define cudaCheck(ans) { cudaAssert((ans), __FILE__, __LINE__); }
inline void cudaAssert(cudaError_t code, const char *file, int line)
{
   if (code != cudaSuccess){
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      exit(code);
   }
}

typedef struct{
  char x;
  char y;
  short value;
} h_element;


h_element h_compact1 [46*19] = {};
h_element h_compact2 [68*30] = {};

__device__ __constant__ h_element dev_h_compact1[46*19];  // used in kernel 1
__device__ __constant__ h_element dev_h_compact2[68*30];  // used in kernel 2

// row and col element count
__device__ __constant__ char h_ele_row_bg1_count[46] = {
	19, 19, 19, 19, 3,  8,  9,  7, 10,  9,
	7,  8,  7,  6,  7,  7,  6,  6,  6,  6,  
	6,  6,  5,  5,  6,  5,  5,  4,  5,  5,  
	5,  5,  5,  5,  5,  5,  5,  4,  5,  5,  
	4,  5,  4,  5,  5,  4};
__device__ __constant__ char h_ele_col_bg1_count[68] = {
	30, 28,  7, 11,  9,  4,  8, 12,  8,  7, 
	12, 10, 12, 11, 10,  7, 10, 10, 13,  7,  
	8,  11, 12,  5,  6,  6,  1,  1,  1,  1,  
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1,  
	1,   1,  1,  1,  1,  1,  1,  1};
__device__ __constant__ char h_ele_row_bg2_count[42] = { 
	8, 10,  8, 10,  4,  6,  6,  6,  4,  5, 
	5,  5,  4,  5,  5,  4,  5,  5,  4,  4,
	4,  4,  3,  4,  4,  3,  5,  3,  4,  3,
	5,  3,  4,  4,  4,  4,  4,  3,  4,  4, 
	4,  4};
__device__ __constant__ char h_ele_col_bg2_count[52] = {
	22, 23, 10,  5,  5, 14,  7, 13,  6,  8, 
	9,  16,  9, 12,  1,  1,  1,  1,  1,  1,
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,   1,  1,  1,  1,  1,  1,  1,  1,  1, 
	1,   1};


__global__ void warmup()
{
	// warm up gpu for time measurement
}


// Kernel 1
__global__ void ldpc_cnp_kernel_1st_iter(char * dev_llr, char * dev_dt, int BG, int row, int col, int Zc)
{
//	if(blockIdx.x == 0 && threadIdx.x == 1) printf("cnp %d\n", threadIdx.x);
	int iMCW = blockIdx.y;		// codeword id
	int iBlkRow = blockIdx.x;	// block row in h_base
	int iBlkCol;				// block col in h_base
	int iSubRow = threadIdx.x;	// row index in sub_block of h_base
	int iCol;					// overall col index in h_base
	int offsetR;
	int shift_t;

//	For 2-min algorithm.
	int Q_sign = 0;
	int sq;
	int Q, Q_abs;
	int R_temp;

	int sign = 1;
	int rmin1 = INT32_MAX;
	int rmin2 = INT32_MAX;
	char idx_min = 0;

	h_element h_element_t;
	int s = (BG==1)? h_ele_row_bg1_count[iBlkRow]:h_ele_row_bg2_count[iBlkRow];
	offsetR = (iMCW * row*col*Zc) + iBlkRow * Zc + iSubRow;	// row*col*Zc = size of dev_dt
//	if(blockIdx.x == 0 && threadIdx.x == 1) printf("s: %d, offset %d\n", s, offsetR);
//	The 1st recursion
	for(int i = 0; i < s; i++) // loop through all the ZxZ sub-blocks in a row
	{
		h_element_t = dev_h_compact1[i*row+iBlkRow];	// compact_col == row

		iBlkCol = h_element_t.y;
		shift_t = h_element_t.value;

		shift_t = (iSubRow + shift_t) % Zc;
		iCol = (iMCW * col*Zc) + iBlkCol * Zc + shift_t;	// col*Zc = size of llr
		Q = dev_llr[iCol];
		Q_abs = (Q>0)? Q : -Q;
		sq = Q < 0;
//		if(blockIdx.x == 0 && threadIdx.x == 1) printf("i %d, icol %d, Q: %d\n", i, iCol, Q);
		// quick version
		sign = sign * (1 - sq * 2);
		Q_sign |= sq << i;

		if (Q_abs < rmin1){
			rmin2 = rmin1;
			rmin1 = Q_abs;
			idx_min = i;
		} else if (Q_abs < rmin2){
			rmin2 = Q_abs;
		}
	}

//	if(blockIdx.x == 0 && threadIdx.x == 1)printf("min1 %d, min2 %d, min1_idx %d\n", rmin1, rmin2, idx_min);

//	The 2nd recursion
	for(int i = 0; i < s; i++){
		// v0: Best performance so far. 0.75f is the value of alpha.
		sq = 1 - 2 * ((Q_sign >> i) & 0x01);
		R_temp = 0.8 * sign * sq * (i != idx_min ? rmin1 : rmin2);
		// write results to global memory
		h_element_t = dev_h_compact1[i*row+iBlkRow];
		int addr_temp = offsetR + h_element_t.y * row * Zc;
		dev_dt[addr_temp] = R_temp;
//		if(blockIdx.x == 0 && threadIdx.x == 1)printf("R_temp %d, temp_addr %d\n", R_temp, addr_temp);
	}	
}

// Kernel_1
__global__ void ldpc_cnp_kernel(char * dev_llr, char * dev_dt, int BG, int row, int col, int Zc)
{
//	if(blockIdx.x == 0 && threadIdx.x == 1) printf("cnp\n");
	int iMCW = blockIdx.y;
	int iBlkRow = blockIdx.x;	// block row in h_base
	int iBlkCol; 				// block col in h_base
	int iSubRow = threadIdx.x;	// row index in sub_block of h_base
	int iCol; 					// overall col index in h_base
	int offsetR;
	int shift_t;

//	For 2-min algorithm.
	int Q_sign = 0;
	int sq;
	int Q, Q_abs;
	int R_temp;

	int sign = 1;
	int rmin1 = INT32_MAX;
	int rmin2 = INT32_MAX;
	char idx_min = 0;

	h_element h_element_t;
	int s = (BG==1)? h_ele_row_bg1_count[iBlkRow]: h_ele_row_bg2_count[iBlkRow];
	offsetR = (iMCW *row*col*Zc) + iBlkRow * Zc + iSubRow;	// row * col * Zc = size of dev_dt
//	if(blockIdx.x == 0 && threadIdx.x == 1) printf("s: %d, offset %d\n", s, offsetR);
//	The 1st recursion
	for(int i = 0; i < s; i++) // loop through all the ZxZ sub-blocks in a row
	{
		h_element_t = dev_h_compact1[i*row+iBlkRow];

		iBlkCol = h_element_t.y;
		shift_t = h_element_t.value;
		shift_t = (iSubRow + shift_t) % Zc;
		iCol = iBlkCol * Zc + shift_t;
		
		R_temp = dev_dt[offsetR + iBlkCol * row * Zc];

		Q = dev_llr[iMCW * (col*Zc) + iCol] - R_temp;
		Q_abs = (Q>0)? Q : -Q;
//		if(blockIdx.x == 0 && threadIdx.x == 1) printf("i %d, icol %d, Q: %d\n", i, iCol, Q);
		sq = Q < 0;
		sign = sign * (1 - sq * 2);
		Q_sign |= sq << i;

		if (Q_abs < rmin1){
			rmin2 = rmin1;
			rmin1 = Q_abs;
			idx_min = i;
		} else if (Q_abs < rmin2){
			rmin2 = Q_abs;
		}
		
	}

//	if(blockIdx.x == 0 && threadIdx.x == 1)printf("min1 %d, min2 %d, min1_idx %d\n", rmin1, rmin2, idx_min);
	
//	The 2nd recursion
	for(int i = 0; i < s; i ++){
		sq = 1 - 2 * ((Q_sign >> i) & 0x01);
		R_temp = 0.8 * sign * sq * (i != idx_min ? rmin1 : rmin2);
		

		// write results to global memory
		h_element_t = dev_h_compact1[i*row+iBlkRow];
		int addr_temp = h_element_t.y * row * Zc + offsetR;
		dev_dt[addr_temp] = R_temp;
//		if(blockIdx.x == 0 && threadIdx.x == 1)printf("R_temp %d, temp_addr %d\n", R_temp, addr_temp);
	}
}

// Kernel 2: VNP processing
__global__ void
ldpc_vnp_kernel_normal(char * dev_llr, char * dev_dt, char * dev_const_llr, int BG, int row, int col, int Zc)
{	
	int iMCW = blockIdx.y;
	int iBlkCol = blockIdx.x;
	int iBlkRow;
	int iSubCol = threadIdx.x;
	int iRow;
	int iCol;

	int shift_t, sf;
	int APP;

	h_element h_element_t;

	// update all the llr values
	iCol = iBlkCol * Zc + iSubCol;
	APP = dev_const_llr[iMCW *col*Zc + iCol];
	int offsetDt = iMCW *row*col*Zc + iBlkCol * row * Zc;
	int s = (BG==1)? h_ele_col_bg1_count[iBlkCol]:h_ele_col_bg2_count[iBlkCol];
	
	for(int i = 0; i < s; i++)
	{
		h_element_t = dev_h_compact2[i*col+iBlkCol];

		shift_t = h_element_t.value%Zc;
		iBlkRow = h_element_t.x;

		sf = iSubCol - shift_t;
		sf = (sf + Zc) % Zc;

		iRow = iBlkRow * Zc + sf;
		APP = APP + dev_dt[offsetDt + iRow];
	}
	if(APP > SCHAR_MAX)	APP = SCHAR_MAX;
	if(APP < SCHAR_MIN)	APP = SCHAR_MIN;
	// write back to device global memory
	dev_llr[iMCW *col*Zc + iCol] = APP;
}


__global__ void pack_decoded_bit(char *dev, unsigned char *host, int col, int Zc)
{
	__shared__ unsigned char tmp[128];
	int iMCW = blockIdx.y;
	int tid = iMCW * col*Zc + blockIdx.x*128 + threadIdx.x;
	int btid = threadIdx.x;
	tmp[btid] = 0;
	
	if(dev[tid] < 0){
		tmp[btid] = 1 << (7-(btid&7));
	}
	__syncthreads();
	
	if(threadIdx.x < 16){
		host[iMCW * col*Zc + blockIdx.x*16+threadIdx.x] = 0;
		for(int i = 0; i < 8; i++){
			host[iMCW * col*Zc + blockIdx.x*16+threadIdx.x] += tmp[threadIdx.x*8+i];
		}
	}
}

void read_BG(int BG, int *h, int row, int col)
{
	int compact_row = 30, compact_col = 19;
	if(BG==2){compact_row = 10, compact_col = 23;}
	
	h_element h_element_temp;

	// init the compact matrix
	for(int i = 0; i < compact_col; i++){
		for(int j = 0; j < row; j++){
			h_element_temp.x = 0;
			h_element_temp.y = 0;
			h_element_temp.value = -1;
			h_compact1[i*row+j] = h_element_temp; // h[i][0-11], the same column
        }
    }
	// scan the h matrix, and gengerate compact mode of h
	for(int i = 0; i < row; i++){
		int k = 0;
		for(int j = 0; j < col; j++){
			if(h[i*col+j] != -1){
				h_element_temp.x = i;
				h_element_temp.y = j;
				h_element_temp.value = h[i*col+j];
				h_compact1[k*row+i] = h_element_temp;
				k++;
            }
        }
    }
	
	// h_compact2
	// init the compact matrix
	for(int i = 0; i < compact_row; i++){
		for(int j = 0; j < col; j++){
			h_element_temp.x = 0;
			h_element_temp.y = 0;
			h_element_temp.value = -1;
			h_compact2[i*col+j] = h_element_temp;
        }
    }

	for(int j = 0; j < col; j++){
		int k=0;
		for(int i = 0; i < row; i++){
			if(h[i*col+j] != -1){
				// although h is transposed, the (x,y) is still (iBlkRow, iBlkCol)
				h_element_temp.x = i; 
				h_element_temp.y = j;
				h_element_temp.value = h[i*col+j];
				h_compact2[k*col+j] = h_element_temp;
				k++;
			}
		}
	}
	
	/*
	for(int i = 0; i < compact_col; i++){
		for(int j = 0; j < row; j++){
			printf("%3d, ", h_compact1[i*row+j].value);
		}
		printf("\n");
	}
	
	for(int i = 0; i < compact_row; i++){
		for(int j = 0; j < col; j++){
			printf("%3d,", h_compact2[i*col+j].value);
		}
		printf("\n");
	}
	*/
}


extern "C"
int32_t nrLDPC_decoder_LYC(t_nrLDPC_dec_params* p_decParams, int8_t* p_llr, int8_t* p_out, int block_length, time_stats_t *time_decoder)
{
	// alloc mem
	//unsigned char *decision = (unsigned char*)p_out;

    uint16_t Zc          = p_decParams->Z;
    uint8_t  BG         = p_decParams->BG;
    uint8_t  numMaxIter = p_decParams->numMaxIter;
    e_nrLDPC_outMode outMode = p_decParams->outMode;
	
	uint8_t row,col;
	if(BG == 1){
		row = 46;
		col = 68;
	}
	else{
		row = 42;
		col = 52;
	}
	int compact_row = 30, compact_col = 19, lift_index=0;;
	if(BG==2){compact_row = 10, compact_col = 23;}
	
	short lift_set[][9] = {
		{2,4,8,16,32,64,128,256},
		{3,6,12,24,48,96,192,384},
		{5,10,20,40,80,160,320},
		{7,14,28,56,112,224},
		{9,18,36,72,144,288},
		{11,22,44,88,176,352},
		{13,26,52,104,208},
		{15,30,60,120,240},
		{0}
	};
	
	for(int i = 0; lift_set[i][0] != 0; i++){
		for(int j = 0; lift_set[i][j] != 0; j++){
			if(Zc == lift_set[i][j]){
				lift_index = i;
				break;
			}
		}
	}
	
	int *h = NULL;
	switch(lift_index){
		case 0:
			h = (BG == 1)? h_base_0:h_base_8;
			break;
		case 1:
			h = (BG == 1)? h_base_1:h_base_9;
			break;
		case 2:
			h = (BG == 1)? h_base_2:h_base_10;
			break;
		case 3:
			h = (BG == 1)? h_base_3:h_base_11;
			break;
		case 4:
			h = (BG == 1)? h_base_4:h_base_12;
			break;
		case 5:
			h = (BG == 1)? h_base_5:h_base_13;
			break;
		case 6:
			h = (BG == 1)? h_base_6:h_base_14;
			break;
		case 7:
			h = (BG == 1)? h_base_7:h_base_15;
			break;
	}
	/* pack BG in compact graph */
	read_BG(BG, h, row, col);
	
	
	int memorySize_h_compact1 = row * compact_col * sizeof(h_element);
	int memorySize_h_compact2 = compact_row * col * sizeof(h_element);

//	cpu
	int memorySize_hard_decision = col * Zc * sizeof(unsigned char) * MC;
	
	
//	alloc memory
	unsigned char *hard_decision = (unsigned char*)p_out;
	
//	gpu
	int memorySize_llr_cuda = col * Zc * sizeof(char) * MC;
	int memorySize_dt_cuda = row * Zc * col * sizeof(char) * MC;


//	alloc memory
	char *dev_llr;
	char *dev_dt;
	char *dev_const_llr;
	unsigned char *dev_tmp;
	
	cudaCheck( cudaMalloc((void **)&dev_tmp, memorySize_hard_decision) );
	cudaCheck( cudaMalloc((void **)&dev_llr, memorySize_llr_cuda) );
	cudaCheck( cudaMalloc((void **)&dev_const_llr, memorySize_llr_cuda) );
	cudaCheck( cudaMalloc((void **)&dev_dt, memorySize_dt_cuda) );
	
//	memcpy host to device	

	cudaCheck( cudaMemcpyToSymbol(dev_h_compact1, h_compact1, memorySize_h_compact1) );
	cudaCheck( cudaMemcpyToSymbol(dev_h_compact2, h_compact2, memorySize_h_compact2) );
	cudaCheck( cudaMemcpy((void*)dev_const_llr, p_llr, memorySize_llr_cuda, cudaMemcpyHostToDevice) );
start_meas(time_decoder);
	cudaCheck( cudaMemcpy((void*)dev_llr, p_llr, memorySize_llr_cuda, cudaMemcpyHostToDevice) );
	

// Define CUDA kernel dimension
	int blockSizeX = Zc;
	dim3 dimGridKernel1(row, MC, 1); 	// dim of the thread blocks
	dim3 dimBlockKernel1(blockSizeX, 1, 1);

    dim3 dimGridKernel2(col, MC, 1);
    dim3 dimBlockKernel2(blockSizeX, 1, 1);	
	cudaDeviceSynchronize();
	
	cudaEvent_t start, end;
	float time;

	warmup<<<dimGridKernel1, dimBlockKernel1>>>();
	warmup<<<dimGridKernel2, dimBlockKernel2>>>();


	cudaEventCreate(&start);
	cudaEventCreate(&end);
	cudaEventRecord(start, 0);
	
//	cudaProfilerStart();
	
// lauch kernel 
	for(int ii = 0; ii < MAX_ITERATION; ii++){
		// first kernel	
		if(ii == 0){
			ldpc_cnp_kernel_1st_iter 
			<<<dimGridKernel1, dimBlockKernel1>>>
			(dev_llr, dev_dt, BG, row, col, Zc);
		}else{
			ldpc_cnp_kernel
			<<<dimGridKernel1, dimBlockKernel1>>>
			(dev_llr, dev_dt, BG, row, col, Zc);
		}
		
		// second kernel
		
			ldpc_vnp_kernel_normal
			<<<dimGridKernel2, dimBlockKernel2>>>
			(dev_llr, dev_dt, dev_const_llr, BG, row, col, Zc);
		
	}
	int pack = (block_length/128)+1;
	dim3 pack_block(pack, MC, 1);
	pack_decoded_bit<<<pack_block,128>>>(dev_llr, dev_tmp, col, Zc);


	cudaEventRecord(end, 0);
	cudaEventSynchronize(end);
	cudaEventElapsedTime(&time, start, end);
	

	//cudaCheck( cudaMemcpy((*)hard_decision, (const void*)dev_tmp, memorySize_hard_decision, cudaMemcpyDeviceToHost) );
	cudaCheck( cudaMemcpy((void*)hard_decision, (const void*)dev_tmp, (block_length/8)*sizeof(unsigned char), cudaMemcpyDeviceToHost) );
	cudaDeviceSynchronize();
stop_meas(time_decoder);
	
	cudaCheck( cudaFree(dev_llr) );
	cudaCheck( cudaFree(dev_dt) );
	cudaCheck( cudaFree(dev_const_llr) );
	cudaCheck( cudaFree(dev_tmp) );
	
	//free(hard_decision);
	return MAX_ITERATION;
	
}
