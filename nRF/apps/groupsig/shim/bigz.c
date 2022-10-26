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
  if (sizeof(op) > sizeof(BN_ULONG)) return ret;

bigz_t bigz_init() {
    errno = EINVAL;
    return IERROR;
}

bigz_t bigz_init_set(bigz_t op) {
    errno = EINVAL;
    return IERROR;
}

bigz_t bigz_init_set_ui(unsigned long int op) {
    errno = EINVAL;
    return IERROR;
}

int bigz_free(bigz_t op) {
    errno = EINVAL;
    return IERROR;
}

int bigz_set(bigz_t rop, bigz_t op) {

    if(!rop || !op) {
        errno = EINVAL;
        return IERROR;
    }

    rop->copy(op);     
    return IOK;
}

int bigz_set_ui(bigz_t rop, unsigned long int op) {

    if(!rop) {
        return IERROR;
    }

    // there used to be a sanity check here...
    
    if(sizeof(unsigned long int) == sizeof(uint32_t)) {
        rop->std_words[0] = op;
    } else if(sizeof(unsigned long int) == sizeof(uint64_t)) {
        rop->std_dwords[0] = op;
    } else {
        return IERROR;
    }
  
    return IOK;
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

    if(op->is_zero()) return 0;
    // check last bit for negative flag
    if(op->bit((op->byte_length * 8) - 1)) return -1;

    return 1;
}

int bigz_cmp(bigz_t op1, bigz_t op2) {

    if(!op1 || !op2) {
        errno = EINVAL;
        return IERROR; /* This does not really matter here. What signals the error is errno */
    }

    // TODO SOOOO SKETCH, definitely broken
    return bigz_t::compare(op1, op2);
}

int bigz_cmp_ui(bigz_t op1, unsigned long int op2) {

    if(!op1) {
        errno = EINVAL;
        return IERROR; /* This does not really matter here. What signals the error is errno */
    }

    bigz_t _op2;
    if(bigz_set_ui(_op2, op2) != IOK) {
        errno = EINVAL;
        return IERROR;
    }
  
    return bigz_cmp(op1, _op2);
}

int bigz_neg(bigz_t rop, bigz_t op) {

    if(!rop || !op) {
        errno = EINVAL;
        return IERROR;
    }

    if (bigz_cmp(rop,op)) {
        rop->copy(op);
    }

    // TODO BROKEN
    op->bytes[bigz_t::byte_length - 1] |= 1 << 7;
    return IOK;
}

int bigz_add(bigz_t rop, bigz_t op1, bigz_t op2) {

    if(!rop || !op1 || !op2) {
        errno = EINVAL;
        return IERROR;
    }

    rop->add(op1, op2);
    return IOK;
}

int bigz_add_ui(bigz_t rop, bigz_t op1, unsigned long int op2) {

    if(!rop || !op1) {
        errno = EINVAL;
        return IERROR;
    }

    bigz_t _op2;
    if(bigz_set_ui(_op2, op2) != IOK) {
        errno = EINVAL;
        return IERROR;
    }

    rop->add(op1, _op2);
    return IOK;
}

int bigz_sub(bigz_t rop, bigz_t op1, bigz_t op2) {

    if(!rop || !op1 || !op2) {
        errno = EINVAL;
        return IERROR;
    }

    rop->subtract(op1, op2);
    return IOK;
}

int bigz_sub_ui(bigz_t rop, bigz_t op1, unsigned long int op2) {

    if(!rop || !op1) {
        errno = EINVAL;
        return IERROR;
    }

    bigz_t _op2;
    if(bigz_set_ui(_op2, op2) != IOK) {
        errno = EINVAL;
        return IERROR;
    }

    rop->subtract(op1, _op2);
    return IOK;
}

int bigz_mul(bigz_t rop, bigz_t op1, bigz_t op2) {

    if(!rop || !op1 || !op2) {
        errno = EINVAL;
        return IERROR;
    }

    rop->multiply(op1, op2);
    return IOK;
}

int bigz_mul_ui(bigz_t rop, bigz_t op1, unsigned long int op2) {

    if(!rop || !op1) {
        errno = EINVAL;
        return IERROR;
    }

    bigz_t _op2;
    if(bigz_set_ui(_op2, op2) != IOK) {
        errno = EINVAL;
        return IERROR;
    }

    rop->multiply(op1, _op2);
    return IOK;
}

int bigz_tdiv(bigz_t q, bigz_t r, bigz_t D, bigz_t d) {

    if(!D || !d || (!q && !r)) {
        errno = EINVAL;
        return IERROR;
    }

    errno = 0;
    if(d->is_zero()) {
        errno = EINVAL;
        return IERROR;
    }

    errno = EINVAL;
    return IERROR;

    /* TODO broke
    // q, r = D / d
    if(!BN_div(q, r, D, d)) {
        return IERROR;    
    }

    return IOK;
    */
}

int bigz_tdiv_ui(bigz_t q, bigz_t r, bigz_t D, unsigned long int d) {

    if(!D || (!q && !r) || !d) {
        errno = EINVAL;
        return IERROR;
    }

    if(sizeof(unsigned long int) == sizeof(bigz_t::word_t)) {
        unsigned long int rem = q->divide_word<d>(D); 
        bigz_set_ui(r, rem);        
    } else if(sizeof(unsigned long int) == sizeof(uint64_t)) {
        uint64_t rem = q->divide_std_dword<d>(D);
        bigz_set_ui(r, rem);
    } else {
        errno = EINVAL;
        return IERROR;
    }

    return IOK;
}

int bigz_divisible_p(bigz_t n, bigz_t d) {

    if(!n || !d) {
        errno = EINVAL;
        return IERROR;
    }

    // TODO NEED DIVISION 
    /*
    bigz_t r;

    if(!(r = bigz_init())) {
    return IERROR;
    }

    if(!BN_mod(r, n, d, sysenv->big_ctx)) {
    bigz_free(r); r = NULL;
    return IERROR;      
    }

    if(BN_is_zero(r)) {
    bigz_free(r); r = NULL;
    return 1;
    }

    bigz_free(r); r = NULL;
    */

    return 0;
}

int bigz_divexact(bigz_t rop, bigz_t n, bigz_t d) {

    errno = EINVAL;
    return IERROR;

    // TODO
  /*
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
  */
}

int bigz_divexact_ui(bigz_t rop, bigz_t n, unsigned long int d) {

    if(!rop || !n) {
        errno = EINVAL;
        return IERROR;
    }

    rop->divide<d>(n);
    return IOK;
}

int bigz_mod(bigz_t rop, bigz_t op, bigz_t mod) {

    errno = EINVAL;
    return IERROR;

    // TODO
  /*
  if(!rop || !op || !mod) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!BN_mod(rop, op, mod, sysenv->big_ctx)) {
    return IERROR;
  }
  
  return IOK;
  */
}

int bigz_powm(bigz_t rop, bigz_t base, bigz_t exp, bigz_t mod) {

    errno = EINVAL;
    return IERROR;    

    // TODO
  /*
  if(!rop || !base || !exp || !mod) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!BN_mod_exp(rop, base, exp, mod, sysenv->big_ctx) == IERROR) {
    return IERROR;
  }
  
  return IOK;
  */

}

int bigz_pow_ui(bigz_t rop, bigz_t base, unsigned long int exp) {

    if(!rop) {
        errno = EINVAL;
        return IERROR;
    }

    for (int b = 0; b < sizeof(exp) * 8; b++) {
        if ((exp >> b) & 0x1) {
            rop->multiply(base);
        }
        base->square();
    }

    return IOK;
}

int bigz_ui_pow_ui(bigz_t rop, unsigned long int base, unsigned long int exp) {

    if(!rop) {
        errno = EINVAL;
        return IERROR;
    }

    bigz_t _base;
    if(bigz_set_ui(_base, base) == IERROR) {
        return IERROR;
    }

    if(bigz_pow_ui(rop, _base, exp) == IERROR) {
        return IERROR;
    }

    return IOK;
}

int bigz_invert(bigz_t rop, bigz_t op, bigz_t mod) {

    // TODO
  /*
  if(!rop || !op || !mod) {
    errno = EINVAL;
    return IERROR;    
  }

  if(!BN_mod_inverse(rop, op, mod, sysenv->big_ctx)) {
    return IERROR;
  }
  
  return IOK;
  */
}

int bigz_probab_prime_p(bigz_t n, int reps) {

    // TODO
  /*
  int rc;
  
  if(!n || !reps) {
    errno = EINVAL;
    return IERROR;
  }

  if((rc = BN_is_prime_ex(n, reps, sysenv->big_ctx, NULL)) == -1) {
    errno = EINVAL;
    return IERROR;
  }

  return rc;
  */

}

int bigz_nextprime(bigz_t rop, bigz_t lower) {

    // TODO
    /*
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

    if(!BN_generate_prime_ex(rop, (int) bits, 0, NULL, NULL, NULL)) {
      return IERROR;      
    }

    errno = 0;
    cmp = bigz_cmp(rop, lower);
    if (errno) {
      return IERROR;    
    }    

  } while(cmp <= 0);
  
  return IOK;
  */

}

int bigz_gcd(bigz_t rop, bigz_t op1, bigz_t op2) {

    // TODO
    /*
  if(!rop || !op1 || !op2) {
    errno = EINVAL;
    return IERROR;
  }

  if(!BN_gcd(rop, op1, op2, sysenv->big_ctx)) {
    return IERROR;     
  }
  
  return IOK;
  */
}

size_t bigz_sizeinbits(bigz_t op) {

    if(!op) {
        errno = EINVAL;
        return IERROR;
    }

    // TODO won't work
    return bigz_t::bits_value;
}

char* bigz_get_str16(bigz_t op) {

  if(!op) {
    errno = EINVAL;
    return NULL;
  }

  return BN_bn2hex(op);

}

int bigz_set_str16(bigz_t rop, char *str) {
  
  if(!rop || !str) {
    errno = EINVAL;
    return IERROR;
  }

  if(!BN_hex2bn(&rop, str)) return IERROR;
  return IOK;

}

char* bigz_get_str10(bigz_t op) {

  if(!op) {
    errno = EINVAL;
    return NULL;
  }

  return BN_bn2dec(op);

}

int bigz_set_str10(bigz_t rop, char *str) {
  
  if(!rop || !str) {
    errno = EINVAL;
    return IERROR;
  }

  if(!BN_dec2bn(&rop, str)) return IERROR;
  return IOK;

}

byte_t* bigz_export(bigz_t op, size_t *length) {

  byte_t *bytes;
  size_t _length;
  
  if(!op || !length) {
    errno = EINVAL;
    return NULL;
  }

  _length = BN_num_bytes(op);
  if(!(bytes = (byte_t *) mem_malloc(sizeof(byte_t)*(_length+1)))) {
    return NULL;
  }

  memset(bytes, 0, sizeof(byte_t)*(_length+1));

  /* BN_bn2bin does not store the sign, and we want it */
  if(bigz_sgn(op) == -1) bytes[0] |= 0x01;

  if(BN_bn2bin(op, &bytes[1]) != _length) {
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

  if(!(bz = BN_bin2bn(&bytearray[1], (int) length-1, NULL))) {
    return NULL;
  }

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

  if(!BN_rand(rop, (int) n, -1, false)) {
    return IERROR;    
  }

  return IOK;

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

  if(!BN_clear_bit(op, (int) index)) {
    return IERROR;
  }

  return IOK;

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

  return BN_is_bit_set(op, (int) index);

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
