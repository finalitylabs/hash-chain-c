#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include<time.h>

#include <CL/cl.h>

// http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

// swap byte endian
uint32_t swapE32(uint32_t val) {
    uint32_t x = val;
    x = (x & 0xffff0000) >> 16 | (x & 0x0000ffff) << 16;
    x = (x & 0xff00ff00) >>  8 | (x & 0x00ff00ff) <<  8;
    return x;
}

uint64_t swapE64(uint64_t val) {
    uint64_t x = val;
    x = (x & 0xffffffff00000000) >> 32 | (x & 0x00000000ffffffff) << 32;
    x = (x & 0xffff0000ffff0000) >> 16 | (x & 0x0000ffff0000ffff) << 16;
    x = (x & 0xff00ff00ff00ff00) >>  8 | (x & 0x00ff00ff00ff00ff) <<  8;
    return x;
}

// print hex numbers
void hex(void* buffer, size_t len) {
    for(size_t i = 0; i < len; i++) {
        printf("%02x", ((char*)buffer)[i] & 0xff);
        if(i % 4 == 3)
            printf(" ");
    }
}

void hexOutput(void* buffer, size_t len) {
    hex(buffer, len);
    printf("\n");
}

int sha256(cl_context context, cl_command_queue q, cl_kernel kernel) {
    uint32_t chains = 1;
    // Generate seed random strings
    int random_string_length = 33;

    char hashes[chains][random_string_length];

    char alphabet[26] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
                          'h', 'i', 'j', 'k', 'l', 'm', 'n',  
                          'o', 'p', 'q', 'r', 's', 't', 'u', 
                          'v', 'w', 'x', 'y', 'z' }; 

    srand(time(0));
    for(int i = 0; i < chains; i++) {
        for (int x = 0; x < random_string_length; x++)
            hashes[i][x] = alphabet[rand() % 26];        
    }

    printf("string: %s\n", hashes[0]);
    printf("string: %s\n", hashes[1]);
    printf("sizeof(hashes[0]): %d\n", sizeof(hashes[0]));
    size_t len = strlen(&hashes[0]) / chains;
    printf("len: %d\n", len);

    // 5.1.1
    uint64_t l = len * sizeof(char) * 8;
    printf("l: %ld\n", l);
    printf("sizeof(char): %ld\n", sizeof(char));
    
    size_t k = (448 - l - 1) % 512;
    printf("k: %d\n", k);
    if(k < 0) k += 512;
    assert((l+1+k) % 512 == 448);

    size_t msgSize = l + 1 + k + 64;
    printf("msgSize: %d\n", msgSize);
    printf("msgSize * chains %d\n", (msgSize / 8) * chains);

    char* msgPad = (char*)calloc((msgSize / 8) * chains, sizeof(char));
    printf("msgPad size allocated memory pointer: %d\n", sizeof(&msgPad[0]));
    //memcpy(msgPad, &sres, len);

    // -------
    // 5.2.1
    uint32_t N = msgSize / 512;
    printf("N:%d\n", N);

    // 6.2
    uint32_t* M = (uint32_t*)msgPad;
    printf("M size allocated memory pointer: %d\n", sizeof(&M[0]));

    for(int i = 0; i < chains; i++) {
        int hashes_start = i*33;
        printf("hashes_start: %d\n", hashes_start);
        printf("msgPad offset: %d\n", (i*(msgSize / 8)));
        //uint32_t* H = (uint32_t*)hashes + hashes_start;
        printf("hashes again: %s\n", hashes[i]);
        memcpy(msgPad + (i*(msgSize / 8)), &hashes[i], len);
        printf("msgPad Char at i: %s\n", &msgPad[i*64]);
        msgPad[len + (i*(msgSize / 8))] = 0x80;
        if(i == 0) {
            l = swapE64(l);
        }
        //l = swapE64(l);
        printf("new l: %d\n", l);
        memcpy(msgPad + ((msgSize/8)-8) + (i*(msgSize / 8)), &l, 8);

        for(size_t x = 0; x < N * 16; x++) {
            //printf("swaping M[%d]\n", x + (i*(msgSize / 8)));
            printf("(i*(msgSize / 8)): %d\n", x+(i*16));
            M[x + (i*16)] = swapE32(M[x + (i*16)]);
            //M[x] = swapE32(M[x]);
        }
    }
    // -------

    // memcpy(msgPad, &hashes, len);
    // msgPad[len] = 0x80;
    // msgPad[len*2] = 0x80;
    // l = swapE64(l);
    // printf("l after swap64: %d\n", l);
    // memcpy(msgPad+(msgSize/8)-8, &l, 8);
    // memcpy(msgPad+((msgSize/8)*2)-8, &l, 8);
    // printf("msgPad+(msgSize/8)-8: %d\n", (msgSize/8)-8);
    // printf("msgPad+(msgSize/8)-8 x2: %d\n", ((msgSize/8)*2)-8);

    // // 5.2.1
    // uint32_t N = msgSize / 512;
    // printf("N:%d\n", N);

    // // 6.2
    // uint32_t* M = (uint32_t*)msgPad;

    // for(size_t i = 0; i < N * 16; i++) {
    //     printf("swaping M[%d]\n", i);
    //     M[i] = swapE32(M[i]);
    // }

    // for(size_t i = 0; i < N * 16; i++) {
    //     printf("swaping M[%d]\n", i+16);
    //     M[i+16] = swapE32(M[i+16]);
    // }

    cl_int ret;
    cl_mem msgMem = clCreateBuffer(context, CL_MEM_READ_ONLY, chains * 16 * N * sizeof(uint32_t), NULL, &ret);
    cl_mem nMem = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(uint32_t), NULL, &ret);
    cl_mem out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, chains * 8 * sizeof(uint32_t), NULL, &ret);

    clEnqueueWriteBuffer(q, msgMem, CL_TRUE, 0, chains * 16 * N * sizeof(uint32_t), M, 0, NULL, NULL);
    clEnqueueWriteBuffer(q, nMem, CL_TRUE, 0, sizeof(uint32_t), &N, 0, NULL, NULL);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&msgMem);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&nMem);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&out);

    size_t totalSize = 1;
    size_t groupSize = 1;
    clEnqueueNDRangeKernel(q, kernel, 1, NULL, &totalSize, &groupSize, 0, NULL, NULL);

    // result
    uint32_t H[8*chains];
    clEnqueueReadBuffer(q, out, CL_TRUE, 0, 8 * sizeof(uint32_t) * chains, H, 0, NULL, NULL);

    for(size_t i = 0; i < 8; i++) {
        H[i] = swapE32(H[i]);
        hex(&H[i], 4);
    }
    printf("\n");

    free(msgPad);
    return 0;
}

int main(int argc, char* argv[]) {
    // if(argc < 2) {
    //     printf("Usage: sha256 <string>\n");
    //     return 0;
    // }

    // load file
    FILE* f = fopen("sha256.cl", "r");
    if(!f) {
        printf("Could not load sha256.cl!\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long clLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* clFile = (char*)malloc(clLen);
    fread(clFile, 1, clLen, f);

    // get system info
    cl_platform_id  platformID = NULL;
    cl_device_id    deviceID = NULL;
    cl_uint         numPlatforms, numDevices;
    cl_int          ret;

    clGetPlatformIDs(1, &platformID, &numPlatforms);
    clGetDeviceIDs(platformID, CL_DEVICE_TYPE_DEFAULT, 1, &deviceID, &numDevices);

    // OpenCL context and command queue
    cl_context context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
    cl_command_queue q = clCreateCommandQueue(context, deviceID, 0, &ret);

    // compile
    cl_program program = clCreateProgramWithSource(context, 1, (const char**)&clFile,
            (const size_t*)&clLen, &ret);
    clBuildProgram(program, 1, &deviceID, NULL, NULL, NULL);
    cl_kernel k = clCreateKernel(program, "sha256", &ret);


    int result = sha256(context, q, k);

    clFlush(q);
    clFinish(q);
    clReleaseKernel(k);
    clReleaseProgram(program);
    clReleaseCommandQueue(q);
    clReleaseContext(context);

    return result;
}
