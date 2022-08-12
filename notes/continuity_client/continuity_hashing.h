#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>  //据说，这个头文件需要从网上下载或者自己手动建立（自行百度）。
#include "hash.h"
#include "pflush.h" //持久化+++

#define ASSOC_NUM 4                       // The number of slots in a bucket
#define SHARED_BUCKETS_NUM 3
#define KEY_LEN 16                        // The maximum length of a key
#define VALUE_LEN 15                      // The maximum length of a value


typedef bool uint1_t;
//bool占用一个字节。bool取值false和true，0为false，非0为true。如果数个bool对象列在一起，可能会各占一个Byte，这取决于编译器。


typedef struct entry{                     // A slot storing a key-value item 
    uint8_t key[KEY_LEN];
    uint8_t value[VALUE_LEN];
} entry;
 /* 欣欣：
uint8_t是用1个字节表示的；
由于 typedef unsigned char uint8_t; 
uint8_t 实际是一个 char, cerr << 会输出 ASCII 码是 67 的字符，而不是 67 这个数字. 

typdef unsigned int uint32_t; 表示uint32_t为32位无符号类型数据, 
*/



typedef struct continuity_bucket               // A bucket
{
    uint32_t indicators;        // ASSOC_NUM是4的情况，前边空位就是4字节。这里，每一个bit都是一个Indicator（1-bit Indicator）, which indicates whether its corresponding slot is empty
    entry slot[ASSOC_NUM];
} continuity_bucket;



typedef struct continuity_segment          // A segment, 含有两个buckets以及中间的SBucket
{
    continuity_bucket bucket[SHARED_BUCKETS_NUM + 2]; //不知道在数组里宏做运算可不可以。也许可以吧，先试试
} continuity_segment;




typedef struct continuity_hash {       // A Continuity hash table
    continuity_segment *segments;      // The Continuity hash table
    continuity_segment *interim_continuity_segments;  //持久化+++   // Used during resizing;   
    uint64_t continuity_item_num;      // The numbers of items stored in the Continuity hash table
    uint64_t segment_num;              // The number of all segments in the Continuity hash table  
    uint64_t continuity_size;          // continuity_size = log2(segment_num). continuity_size就是2的指数
    uint8_t continuity_expand_time;    // Indicate whether the Continuity hash table was expanded, ">1 or =1": Yes, "0": No;
    uint8_t resize_state;              // Indicate the resizing state of the level hash table, ‘0’ means the hash table is not during resizing; 
                                       // ‘1’ means the hash table is being expanded; ‘2’ means the hash table is being shrunk.
    uint64_t seed;                      // A randomized seed for a hash function
} continuity_hash;



continuity_hash *continuity_init(uint64_t continuity_size);
void continuity_expand(continuity_hash *continuity);
void continuity_shrink(continuity_hash *continuity);
uint8_t continuity_insert(continuity_hash *continuity, uint8_t *key, uint8_t *value);
uint8_t* continuity_query(continuity_hash *continuity, uint8_t *key);
uint8_t continuity_delete(continuity_hash *continuity, uint8_t *key);
uint8_t continuity_update(continuity_hash *continuity, uint8_t *key, uint8_t *new_value);
void continuity_destroy(continuity_hash *continuity);

// 欣欣写完了

    

       










