/* 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdlib.h>
#include <limits.h>
#include "sysenv.h"
#include "bigz.h"
#include "sys/mem.h"
//#include "bigf.h"

#define CHECK_ULONG_SIZE(op, ret)   \
  if (sizeof(op) > sizeof(unsigned long)) return ret; 

bigz_t bigz_init() {

  bigz_t bz; 
  mbedtls_mpi_init(&bz)

  return bz;

}

bigz_t bigz_init_set(bigz_t op) {

  if(!op) {
    return NULL; 
  }

  X = bigz_init()
  mbedtls_mpi_copy(&X, &(const bigz_t)op);
  return X
}

bigz_t bigz_init_set_ui(unsigned long int op) {

  bigz_t bz;

  if(!(bz = bigz_init())) {
    return NULL;
  }

  if(!mbedtls_mpi_lset(&bz, op)) ( // returns 0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return bz;
  ) else {
    return NULL; 
  }

}

int bigz_free(bigz_t op) {
  
  /* If there is nothing to free, ok, but throw warning... */
  if(!op) {
    errno = EINVAL;
    return IERROR;
  }

  mbedtls_mpi_free(op);
  op = NULL;
  
  return IOK;

}

int bigz_set(bigz_t rop, bigz_t op) {

  if(!rop || !op) {
    errno = EINVAL;
    return IERROR;
  }

  mbedtls_mpi_copy(&X, &op);

  return IOK;

}

int bigz_set_ui(bigz_t rop, unsigned long int op) {

  if(!rop) {
    return IERROR;
  }

  if(!mbedtls_mpi_lset(&rop, op)) ( // returns 0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IERROR;
  ) else {
    return IOK; 
  }  

  

}

/* int bigz_set_f(bigz_t z, bigf_t f) { */

/*   if(!z || !f) { */
/*     LOG_EINVAL(&logger, __FILE__, "bigz_set_f", __LINE__, LOGERROR); */
/*     return IERROR; */
/*   } */

/*   mpz_set_f(*z, *f); */
  
/*   return IOK; */

/* } */

int bigz_sgn(bigz_t op) {

  if(!op) {
    errno = EINVAL;
    return IERROR;
  }
  
  // 1 if X is greater than z, -1 if X is lesser than z or 0 if X is equal to z
  return mbedtls_mpi_cmp_int(&op, 0) 

}

int bigz_cmp(bigz_t op1, bigz_t op2) {

  if(!op1 || !op2) {
    errno = EINVAL;
    return IERROR; /* This does not really matter here. What signals the error is errno */
  }

  return mbedtls_mpi_cmp_mpi(&op1, &op2);

}

int bigz_cmp_ui(bigz_t op1, unsigned long int op2) {

  bigz_t _op2;
  int rc;
  
  if(!op1) {
    errno = EINVAL;
    return IERROR; /* This does not really matter here. What signals the error is errno */
  }

  if(!(_op2 = bigz_init_set_ui(op2))) {
    return IERROR;
  }

  errno = 0;
  rc = bigz_cmp(op1, _op2);
  bigz_free(_op2); _op2 = NULL;

  return rc;

}

int bigz_neg(bigz_t rop, bigz_t op) {

  if(!rop || !op) {
    errno = EINVAL;
    return IERROR;
  }

  if (rop != op) {
    mbedtls_mpi_copy(&rop, &op);
  }

  if (!mbedtls_mpi_mul_int(rop, -1)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_add(bigz_t rop, bigz_t op1, bigz_t op2) {

  if(!rop || !op1 || !op2) {
    errno = EINVAL;
    return IERROR;
  }

  if (!mbedtls_mpi_add_mpi(&rop, &op1, &op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_add_ui(bigz_t rop, bigz_t op1, unsigned long int op2) {

  if(!rop || !op1) {
    errno = EINVAL;
    return IERROR;
  } 

  if (rop != op1) {
    mbedtls_mpi_copy(&rop, &op);
  }  

  if (!mbedtls_mpi_add_int(&rop, &op1, op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_sub(bigz_t rop, bigz_t op1, bigz_t op2) {

  if(!rop || !op1 || !op2) {
    errno = EINVAL;
    return IERROR;    
  }
  
  if (!mbedtls_mpi_sub_mpi(&rop, &op1, &op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_sub_ui(bigz_t rop, bigz_t op1, unsigned long int op2) {

  if(!rop || !op1) {
    errno = EINVAL;
    return IERROR;    
  }

  if (rop != op1) {
    mbedtls_mpi_copy(&rop, &op);
  }

  if (!mbedtls_mpi_sub_int(&rop, &op1, op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }
  
}

int bigz_mul(bigz_t rop, bigz_t op1, bigz_t op2) { 

  if(!rop || !op1 || !op2) {
    errno = EINVAL;
    return IERROR;    
  }

  if (!mbedtls_mpi_mul_mpi(&rop, &op1, &op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_mul_ui(bigz_t rop, bigz_t op1, unsigned long int op2) { 

  if(!rop || !op1) {
    errno = EINVAL;
    return IERROR;
  }

  if (rop != op1) {
    mbedtls_mpi_copy(&rop, &op);
  }

  if (!mbedtls_mpi_mul_int(&rop, &op1, op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }
  
}

int bigz_tdiv(bigz_t q, bigz_t r, bigz_t D, bigz_t d) { 

  if(!D || !d || (!q && !r)) {
    errno = EINVAL;
    return IERROR;
  }

  errno = 0;
  if(!bigz_cmp_ui(d, 0) || errno) {
    errno = EINVAL;
    return IERROR;
  }

  if(!mbedtls_mpi_div_mpi(&q, &r, &D, &d)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_tdiv_ui(bigz_t q, bigz_t r, bigz_t D, unsigned long int d) { 

  bigz_t _d;
  
  if(!D || (!q && !r)) {
    errno = EINVAL;
    return IERROR;
  }

  if(!d) {
    errno = EINVAL;
    return IERROR;
  }

  if(!(_d = bigz_init_set_ui(d))) {
    return IERROR;
  }

  if(bigz_tdiv(q, r, D, _d) == IERROR) {
    bigz_free(_d); _d = NULL;
    return IERROR;
  }

  bigz_free(_d); _d = NULL;

  return IOK;

  
}

int bigz_divisible_p(bigz_t n, bigz_t d) { //TODO

  bigz_t r;
  
  if(!n || !d) {
    errno = EINVAL;
    return IERROR;
  }

  if(!(r = bigz_init())) {
    return IERROR;
  }

  if(mbedtls_mpi_mod_mpi(&r, &n, &d)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    bigz_free(r); r = NULL;
    return IERROR;      
  }

  if(mbedtls_mpi_cmp_int(&r, 0)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    bigz_free(r); r = NULL;
    return 1;
  }

  bigz_free(r); r = NULL;
  
  return 0;

}

int bigz_divexact(bigz_t rop, bigz_t n, bigz_t d) {

  bigz_t r;

  if(!rop || !n || !d) {
    errno = EINVAL;
    return IERROR;
  }

  if(!(r = bigz_init())) {
    return IERROR;
  }

  if(bigz_tdiv(rop, r, n, d) == IERROR) {
    bigz_free(r); r = NULL;
    return IERROR;
  }

  bigz_free(r); r = NULL;  

  return IOK;

}

int bigz_divexact_ui(bigz_t rop, bigz_t n, unsigned long int d) {

  bigz_t _d, r;
  
  if(!rop || !n) {
    errno = EINVAL;
    return IERROR;
  }

  if(!d) {
    errno = EINVAL;
    return IERROR;
  }

  if(!(_d = bigz_init_set_ui(d))) {
    return IERROR;
  }

  if(!(r = bigz_init())) {
    bigz_free(_d); _d = NULL;
    return IERROR;
  }

  if(bigz_divexact(rop, n, _d) == IERROR) {
    bigz_free(r); r = NULL;
    bigz_free(_d); _d = NULL;
    return IERROR;
  }

  bigz_free(r); r = NULL;
  bigz_free(_d); _d = NULL;

  return IOK;

}

int bigz_mod(bigz_t rop, bigz_t op, bigz_t mod) { 

  if(!rop || !op || !mod) {
    errno = EINVAL;
    return IERROR;    
  }
  
  if(!mbedtls_mpi_mod_mpi(&rop, &op, &mod)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IERROR;
  } else {
    return IOK;
  }

}

int bigz_powm(bigz_t rop, bigz_t base, bigz_t exp, bigz_t mod) { 

  if(!rop || !base || !exp || !mod) {
    errno = EINVAL;
    return IERROR;    
  }

  // apparently setting the NULL value to something can help with speedups for recomputing R*R mod N 
  if(!mbedtls_mpi_exp_mod(&rop, &base, &exp, &mod, NULL)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_pow_ui(bigz_t rop, bigz_t base, unsigned long int exp) { //TODO
  
  if(!rop) {
    errno = EINVAL;
    return IERROR;
  }
  
  //TODO: check if you can use bigz_mul like this...
  for (int b = 0; b < sizeof(exp) * 8; b++) {
    if ((exp >> b) & 0x1) {
      if (bigz_mul(rop, rop, base) == IERROR) {
        return IERROR;
      } 
    }
    //Square the base to get to the next bit
    if (bigz_mul(base, base, base) == IERROR) {
      return IERROR;
    }
  }

  return IOK;

}

int bigz_ui_pow_ui(bigz_t rop, unsigned long int base, unsigned long int exp) {

  bigz_t _base;
  
  if(!rop) {
    errno = EINVAL;
    return IERROR;
  }

  if(!(_base = bigz_init_set_ui(base))) {
    return IERROR;
  }

  if(bigz_pow_ui(rop, _base, exp) == IERROR) {
    bigz_free(_base); _base = NULL;
    return IERROR;
  }

  bigz_free(_base); _base = NULL;
  
  return IOK;

}

int bigz_invert(bigz_t rop, bigz_t op, bigz_t mod) { //TODO

  if(!rop || !op || !mod) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!mbedtls_mpi_inv_mod(&rop, &op, &mod)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }
}

int bigz_probab_prime_p(bigz_t n, int reps) {

  int rc;
  
  if(!n || !reps) {
    errno = EINVAL;
    return IERROR;
  }

  rc = mbedtls_mpi_is_prime(&n, reps); //TODO: might need p_rng and f_rng idk how to get around this right now
  if (rc == MBEDTLS_ERR_MPI_ALLOC_FAILED) {
    return IERROR;
  } else if (rc == MBEDTLS_ERR_MPI_NOT_ACCEPTABLE) {
    // is not prime
    return 0; 
  } else if (rc == 0) {
    // is prime with error probability less than 0.25^nchecks 
    return 1;
  } else {
    return IERROR;
  }

}

int bigz_nextprime(bigz_t rop, bigz_t lower) {

  size_t bits;
  int cmp;
  
  if(!rop || !lower) {
    errno = EINVAL;
    return IERROR;
  }

  errno = 0;
  bits = bigz_sizeinbits(lower);
  if (errno) {
    return IERROR;    
  }

  if (bits > INT_MAX) {
    return IERROR;    
  }

  do {

    // TODO: figure out if flag value actually should be 0 and if we need f_rng and p_rng
    if(mbedtls_mpi_gen_prime(&rop, (int) bits, 0, NULL, NULL)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
      return IERROR;
    }

    errno = 0;
    cmp = bigz_cmp(rop, lower);
    if (errno) {
      return IERROR;    
    }    

  } while(cmp <= 0);
  
  return IOK;

}

int bigz_gcd(bigz_t rop, bigz_t op1, bigz_t op2) {

  if(!rop || !op1 || !op2) {
    errno = EINVAL;
    return IERROR;
  }

  if(!mbedtls_mpi_gcd(&rop, &op1, &op2)) { //0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return IOK;
  } else {
    return IERROR;
  }

}

size_t bigz_sizeinbits(bigz_t op) {

  if(!op) {
    errno = EINVAL;
    return IERROR;
  }
  
  return mbedtls_mpi_bitlen(&op);

}

char* bigz_get_str16(bigz_t op) { //TODO might need to write a hex to binary function

  if(!op) {
    errno = EINVAL;
    return NULL;
  }

  char buf[MBEDTLS_MPI_RW_BUFFER_SIZE];
  memset(buf, 0, sizeof(buf));
  size_t n;
  return mbedtls_mpi_write_string(&op, 16, buff, sizeof(buff) -2, &n); 

}

int bigz_set_str16(bigz_t rop, char *str) { //TODO
  
  if(!rop || !str) {
    errno = EINVAL;
    return IERROR;
  }

  if (!mbedtls_mpi_read_string(&op, 16, str); {// base 16 for hex 
    return IOK;
  } else {
    return IERROR;
  }

}

char* bigz_get_str10(bigz_t op) { //TODO binary to decimal function

  if(!op) {
    errno = EINVAL;
    return NULL;
  }

  char buf[MBEDTLS_MPI_RW_BUFFER_SIZE];
  memset(buf, 0, sizeof(buf));
  size_t n;
  return mbedtls_mpi_write_string(&op, 10, buff, sizeof(buff) -2, &n); // base 10 for decimal

}

int bigz_set_str10(bigz_t rop, char *str) {
  
  if(!rop || !str) {
    errno = EINVAL;
    return IERROR;
  }

  if (!mbedtls_mpi_read_string(&op, 10, str)) { // base 10 for decimal 
  return IOK;
  } else {
  return IERROR;

}

byte_t* bigz_export(bigz_t op, size_t *length) {

  byte_t *bytes;
  size_t _length;
  
  if(!op || !length) {
    errno = EINVAL;
    return NULL;
  }

  _length = mbedtls_mpi_size(&op); 
  if(!(bytes = (byte_t *) mem_malloc(sizeof(byte_t)*(_length+1)))) {
    return NULL;
  }

  memset(bytes, 0, sizeof(byte_t)*(_length+1));

  /* bn2bin does not store the sign, and we want it */
  if(bigz_sgn(op) == -1) bytes[0] |= 0x01;

  if(mbed_mpi_write_binary(&op, &bytes[1], sizeof(bytes[1]))) { // 0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    mem_free(bytes); bytes = NULL;
    return NULL;
  }

  *length = (_length+1);

  return bytes;
    
}

bigz_t bigz_import(byte_t *bytearray, size_t length) {

  bigz_t bz;

  if(!bytearray || length < 1 || length > INT_MAX) {
    errno = EINVAL;
    return NULL;
  }

  if (mbedtls_mpi_read_binary(&bz, &bytearray[1], (int) length-1)) { // 0 if successful, MBEDTLS_ERR_MPI_ALLOC_FAILED if memory allocation failed
    return NULL;
  };

  if(bytearray[0] != 0x00) {
    if(bigz_neg(bz, bz) == IERROR) {
      bigz_free(bz); bz = NULL;
      return NULL;
    }
  }

  return bz;

}

int bigz_dump_bigz_fd(bigz_t z, FILE* fd){

  byte_t *bytes;
  size_t size;
  
  if (!z || !fd) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!(bytes = bigz_export(z, &size))) {
    return IERROR;
  }

  /* Write the size */
  fwrite(&size, sizeof(size_t), 1, fd);

  /* Write the BN */
  fwrite(bytes, 1, size, fd);

  mem_free(bytes); bytes = NULL;

  return IOK;
}


bigz_t bigz_get_bigz_fd(FILE *fd) {

  bigz_t z;
  byte_t *bytes;
  size_t size;
  
  if (!fd) {
    errno = EINVAL;
    return NULL;    
  }

  /* Read the size */
  fread(&size, sizeof(size_t), 1, fd);

  if(!(bytes = (byte_t *) mem_malloc(sizeof(byte_t)*size))) {
    return NULL;
  }

  memset(bytes, 0, sizeof(byte_t)*size);
  if(fread(bytes, 1, size, fd) != size) {
    errno = EBADF;
    mem_free(bytes); bytes = NULL;
    return NULL;
  }

  if(!(z = bigz_import(bytes, size))) {
    mem_free(bytes); bytes = NULL;
    return NULL;
  }

  mem_free(bytes); bytes = NULL;
  
  return z;
  
}

/* void bigz_randinit_default(bigz_randstate_t rand) { */
/*   return gmp_randinit_default(rand); */
/* } */

/* void bigz_randclear(bigz_randstate_t rand) { */
/*   return gmp_randclear(rand); */
/* } */

/* void bigz_randseed_ui(bigz_randstate_t rand, unsigned long int seed) { */
/*   return gmp_randseed_ui(rand, seed); */
/* } */

int bigz_urandomm(bigz_t rop, bigz_t n) {

  size_t size;
  int rc;
  
  if(!rop || !n) {
    errno = EINVAL;
    return IERROR;
  }

  errno = 0;
  size = bigz_sizeinbits(n);
  if (errno) {
    return IERROR;        
  }

  if (size > INT_MAX) {
    errno = EINVAL;
    return IERROR;
  }

  do {
    if (bigz_urandomb(rop, (unsigned long int) size) == IERROR) return IERROR;
    errno = 0;
    rc = bigz_cmp(rop, n);
    if (errno) return IERROR;
  } while (rc >= 0);

  return IOK;
  
}

int bigz_urandomb(bigz_t rop, unsigned long int n) {
  
  if(!rop) {
    errno = EINVAL;
    return IERROR;
  }

  if (n > INT_MAX) {
    errno = EINVAL;
    return IERROR;    
  }

  //TODO: idk about how to do int top and bottom like openssl does maybe not necessary/ maybe a speed up?
  if(!mbedtls_mpi_fill_random(&rop, (size_t) n)) { //TODO needs f_rng and p_rng
    return IOK;    
  } else {
    return IERROR;
  }

}

int bigz_clrbit(bigz_t op, unsigned long int index) {

  if(!op) {
    errno = EINVAL;
    return IERROR;
  }

  if (index > INT_MAX) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!mbedtls_mpi_set_bit(&op, (int) index, 0)) {
    return IOK;
  } else {
    return IERROR;
  }

}

int bigz_tstbit(bigz_t op, unsigned long int index) {

  if(!op) {
    errno = EINVAL;
    return -1;
  }

  if (index > INT_MAX) {
    errno = EINVAL;
    return -1;    
  }

  return mbedtls_mpi_get_bit(&op, (int) index);

}

/* Not just shim... */
/* int bigz_log2(bigf_t log2n, bigz_t n, uint64_t precission) { */

/*   bigf_t f_n; */

/*   if(!n || !log2n) { */
/*     LOG_EINVAL(&logger, __FILE__, "bigz_log2", __LINE__, LOGERROR); */
/*     return IERROR; */
/*   } */

/*   if(!(f_n = bigf_init())) { */
/*     return IERROR; */
/*   } */
  
/*   if(bigf_set_z(f_n, n) == IERROR) { */
/*     bigf_free(f_n); */
/*     return IERROR; */
/*   } */

/*   if(bigf_log2(log2n, f_n, precission) == IERROR) { */
/*     bigf_free(f_n); */
/*     return IERROR; */
/*   } */

/*   bigf_free(f_n); */

/*   return IOK; */

/* } */


/* bigz.c ends here */