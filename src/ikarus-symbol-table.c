/*
 *  Ikarus Scheme -- A compiler for R6RS Scheme.
 *  Copyright (C) 2006,2007,2008  Abdulaziz Ghuloum
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 *  
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "ikarus-data.h"
#include <strings.h>
#include <string.h>
#include <stdlib.h>

static ikptr
make_symbol_table(ikpcb* pcb){
  #define NUM_OF_BUCKETS 4096 /* power of 2 */
  int size = align_to_next_page(disp_vector_data + NUM_OF_BUCKETS * wordsize);
  ikptr st = ik_mmap_ptr(size, 0, pcb) + vector_tag;
  bzero((char*)(long)st-vector_tag, size);
  ref(st, off_vector_length) = fix(NUM_OF_BUCKETS);
  return st;
}

static long int 
compute_hash(ikptr str){
  long int len = unfix(ref(str, off_string_length));
  char* data = (char*)(long) str + off_string_data;
  long int h = len;
  char* last = data + len * string_char_size;
  while(data < last){
    char c = *data;
    h = h + c;
    h = h + (h << 10);
    h = h ^ (h >> 6);
    data++;
  }
  h = h + (h << 3);
  h = h ^ (h >> 11);
  h = h + (h << 15);
  return h;
}

ikptr 
ikrt_string_hash(ikptr str){
  return (ikptr)(compute_hash(str) & (~ fx_mask));
}

static int strings_eqp(ikptr str1, ikptr str2){
  ikptr len = ref(str1, off_string_length);
  if(len == ref(str2, off_string_length)){
    return
      (memcmp((char*)(long)str1+off_string_data, 
              (char*)(long)str2+off_string_data, 
              unfix(len) * string_char_size)
       == 0);
  }
  return 0;
}

#if 0
static ikptr 
ik_make_symbol(ikptr str, ikptr ustr, ikpcb* pcb){
  ikptr sym = ik_unsafe_alloc(pcb, symbol_size) + symbol_tag;
  ref(sym, off_symbol_string)  = str;
  ref(sym, off_symbol_ustring) = ustr;
  ref(sym, off_symbol_value)   = unbound_object;
  ref(sym, off_symbol_plist)   = null_object;
  ref(sym, off_symbol_system_value) = str;
  ref(sym, off_symbol_code) = 0;
  ref(sym, off_symbol_errcode) = 0;
  ref(sym, off_symbol_unused) = 0;
  return sym;
}
#endif

static ikptr 
ik_make_symbol(ikptr str, ikptr ustr, ikpcb* pcb){
  ikptr sym = ik_unsafe_alloc(pcb, symbol_record_size) + record_tag;
  ref(sym, -record_tag) = symbol_record_tag;
  ref(sym, off_symbol_record_string)  = str;
  ref(sym, off_symbol_record_ustring) = ustr;
  ref(sym, off_symbol_record_value)   = unbound_object;
  ref(sym, off_symbol_record_proc)    = str;
  ref(sym, off_symbol_record_plist)   = null_object;
  return sym;
}



static ikptr
intern_string(ikptr str, ikptr st, ikpcb* pcb){
  int h = compute_hash(str);
  int idx = h & (unfix(ref(st, off_vector_length)) - 1);
  ikptr bckt = ref(st, off_vector_data + idx*wordsize);
  ikptr b = bckt;
  while(b){
    ikptr sym = ref(b, off_car);
    ikptr sym_str = ref(sym, off_symbol_record_string);
    if(strings_eqp(sym_str, str)){
      return sym;
    }
    b = ref(b, off_cdr);
  }
  ikptr sym = ik_make_symbol(str, false_object,  pcb);
  b = ik_unsafe_alloc(pcb, pair_size) + pair_tag;
  ref(b, off_car) = sym;
  ref(b, off_cdr) = bckt;
  ref(st, off_vector_data + idx*wordsize) = b;
  ((int*)(long)pcb->dirty_vector)[page_index(st+off_vector_data+idx*wordsize)] = -1;
  return sym;
}

static ikptr
intern_unique_string(ikptr str, ikptr ustr, ikptr st, ikpcb* pcb){
  int h = compute_hash(ustr);
  int idx = h & (unfix(ref(st, off_vector_length)) - 1);
  ikptr bckt = ref(st, off_vector_data + idx*wordsize);
  ikptr b = bckt;
  while(b){
    ikptr sym = ref(b, off_car);
    ikptr sym_ustr = ref(sym, off_symbol_record_ustring);
    if(strings_eqp(sym_ustr, ustr)){
      return sym;
    }
    b = ref(b, off_cdr);
  }
  ikptr sym = ik_make_symbol(str, ustr, pcb);
  b = ik_unsafe_alloc(pcb, pair_size) + pair_tag;
  ref(b, off_car) = sym;
  ref(b, off_cdr) = bckt;
  ref(st, off_vector_data + idx*wordsize) = b;
  ((int*)(long)pcb->dirty_vector)[page_index(st+off_vector_data+idx*wordsize)] = -1;
  return sym;
}

ikptr
ikrt_intern_gensym(ikptr sym, ikpcb* pcb){
  ikptr st = pcb->gensym_table;
  if(st == 0){
    st = make_symbol_table(pcb);
    pcb->gensym_table = st;
  }
  ikptr ustr = ref(sym, off_symbol_record_ustring);
  int h = compute_hash(ustr);
  int idx = h & (unfix(ref(st, off_vector_length)) - 1);
  ikptr bckt = ref(st, off_vector_data + idx*wordsize);
  ikptr b = bckt;
  while(b){
    ikptr sym = ref(b, off_car);
    ikptr sym_ustr = ref(sym, off_symbol_record_ustring);
    if(strings_eqp(sym_ustr, ustr)){
      return false_object;
    }
    b = ref(b, off_cdr);
  }
  b = ik_unsafe_alloc(pcb, pair_size) + pair_tag;
  ref(b, off_car) = sym;
  ref(b, off_cdr) = bckt;
  ref(st, off_vector_data + idx*wordsize) = b;
  ((int*)(long)pcb->dirty_vector)[page_index(st+off_vector_data+idx*wordsize)] = -1;
  return true_object;
}




ikptr 
ikrt_string_to_symbol(ikptr str, ikpcb* pcb){
  ikptr st = pcb->symbol_table;
  if(st == 0){
    st = make_symbol_table(pcb);
    pcb->symbol_table = st;
  }
  return intern_string(str, st, pcb);
}

ikptr 
ik_intern_string(ikptr str, ikpcb* pcb){
  return ikrt_string_to_symbol(str, pcb);
}

ikptr 
ikrt_strings_to_gensym(ikptr str, ikptr ustr, ikpcb* pcb){
  ikptr st = pcb->gensym_table;
  if(st == 0){
    st = make_symbol_table(pcb);
    pcb->gensym_table = st;
  }
  return intern_unique_string(str, ustr, st, pcb);
}


#if 0
ikptr
ik_cstring_to_symbol(char* str, ikpcb* pcb){
  int n = strlen(str);
  int size = n + disp_string_data + 1;
  ikptr s = ik_unsafe_alloc(pcb, align(size)) + string_tag;
  ref(s, off_string_length) = fix(n);
  memcpy(s+off_string_data, str, n+1);
  ikptr sym = ikrt_string_to_symbol(s, pcb);
  return sym;
}
#endif
