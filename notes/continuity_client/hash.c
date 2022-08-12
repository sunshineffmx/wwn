#include "hash.h"

#define NUMBER64_1 11400714785074694791ULL
#define NUMBER64_2 14029467366897019727ULL
#define NUMBER64_3 1609587929392839161ULL
#define NUMBER64_4 9650029242287828579ULL
#define NUMBER64_5 2870177450012600261ULL

#define hash_get64bits(x) hash_read64_align(x, align)
#define hash_get32bits(x) hash_read32_align(x, align)
#define shifting_hash(x, r) ((x << r) | (x >> (64 - r)))
#define TO64(x) (((U64_INT *)(x))->v)
#define TO32(x) (((U32_INT *)(x))->v)



/*
欣欣：

运算符 含义  描述
<<  左移  用来将一个数的各二进制位全部左移N位，高位舍弃，低位补0。
>>  右移  将一个数的各二进制位右移N位，移到右端的低位被舍弃，对于无符号数，高位补0。
&   按位与 如果两个相应的二进制位都为1，则该位的结果值为1，否则为0。
l   按位或 两个相应的二进制位中只要有一个为1，该位的结果值为1，否则为0。

shifting_hash 相当于是把64位的数字X的前（高）r位，放到了低位处，例如假设r=3，x=110********，则shifting_hash后值变为********110  。
可对于32位数字X就不成立，因为x << r和x >> (64 - r)中至少有一个会变成全零，最后结果大概是：r小的时候，shifting_hash后值变为x << r；r大的时候，shifting_hash后值变为x >> (64 - r)
        

void*类型指针 
由于void是空类型，因此void*类型的指针只保存了指针的值，而丢失了类型信息，我们不知道他指向的数据是什么类型的，只知道这个数据在内存中的起始地址，如果想要完整的提取指向的数据，程序员就必须对这个指针做出正确的类型转换，然后再解指针。......编译器不允许直接对void*类型的指针做解指针操作。


用指针引用结构体变量成员方式总结与技巧：
一、(*指针变量名).成员名
注意，*p 两边的括号不可省略，因为成员运算符“.”的优先级高于指针运算符“*”
二、直接用：指针变量名->成员名
p->num 的含义是：指针变量 p 所指向的结构体变量中的 num 成员。


疑问？？？：
已知#define hash_get64bits(x) hash_read64_align(x, align)
但函数uint64_t string_key_hash_computation(const void *data, uint64_t length, uint64_t seed, uint32_t align)中直接用了hash_get64bits(p)，那参数align的值可以被传递？
？？？？
为什么呀，为什么data后三位是0，那么align就被赋值为1？从而接着hash_read64_align或者叫hash_get64bits返回的就是ptr指针指向的值 *(uint64_t *)ptr，而不是结构体U64_INT中的成员变量的数值TO64(ptr)或者uint64_t v？ 感觉本质都是一样的，都是一个uint64_t格式的数字呀，也就是数字的来源不同，一个是结构体另一个不是？


*/



typedef struct U64_INT
{
    uint64_t v;
} U64_INT;

typedef struct U32_INT
{
    uint32_t v;
} U32_INT;

uint64_t hash_read64_align(const void *ptr, uint32_t align)
{
    if (align == 0)
    {
        return TO64(ptr);
    }
    return *(uint64_t *)ptr;
}

uint32_t hash_read32_align(const void *ptr, uint32_t align)
{
    if (align == 0)
    {
        return TO32(ptr);
    }
    return *(uint32_t *)ptr;
}

/*
Function: string_key_hash_computation() 
        A hash function for string keys

 欣欣：
        指针是程序数据在内存中的地址。
        指针的值实质是内存单元（即字节）的编号。
        由于内存中的每一个字节都有一个唯一的编号...这个编号就是这个数据的地址。指针就是这样形成的。

const 和 指针
   如果const 后面是一个类型，则跳过最近的原子类型，修饰后面的数据。（原子类型是不可再分割的类型，如int, short , char，以及typedef包装后的类型）。
   如果const后面就是一个数据，则直接修饰这个数据。
   例子：
    int a = 1;
   
    int const *p1 = &a;        //const后面是*p1，实质是数据a，则修饰*p1，通过p1不能修改a的值
    const int*p2 =  &a;        //const后面是int类型，则跳过int ，修饰*p2， 效果同上
    int* const p3 = NULL;      //const后面是数据p3。也就是指针p3本身是const .
    const int* const p4 = &a;  // 通过p4不能改变a 的值，同时p4本身也是 const
    int const* const p5 = &a;  //效果同上


p= p+1 意思是，让p指向原来指向的内存块的下一个相邻的相同类型的内存块。
同一个数组中，元素的指针之间可以做减法运算，此时，指针之差等于下标之差。

a^=b; 相当于 a=a^b; 表示a等于a原来的值与b按位异或。
异或运算符"∧"也称XOR运算符。它的规则是若参加运算的两个二进位同号，则结果为0（假）；异号则为1（真）。即 0∧0＝0，0∧1＝1， 1^0=1，1∧1＝0。


*/
uint64_t string_key_hash_computation(const void *data, uint64_t length, uint64_t seed, uint32_t align)
{
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + length;
    uint64_t hash;

    if (length >= 32)
    {
        const uint8_t *const limitation = end - 32;
        uint64_t v1 = seed + NUMBER64_1 + NUMBER64_2;
        uint64_t v2 = seed + NUMBER64_2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - NUMBER64_1;

        do
        {
            v1 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v1 = shifting_hash(v1, 31);
            v1 *= NUMBER64_1;
            v2 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v2 = shifting_hash(v2, 31);
            v2 *= NUMBER64_1;
            v3 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v3 = shifting_hash(v3, 31);
            v3 *= NUMBER64_1;
            v4 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v4 = shifting_hash(v4, 31);
            v4 *= NUMBER64_1;
        } while (p <= limitation);

        hash = shifting_hash(v1, 1) + shifting_hash(v2, 7) + shifting_hash(v3, 12) + shifting_hash(v4, 18);

        v1 *= NUMBER64_2;
        v1 = shifting_hash(v1, 31);
        v1 *= NUMBER64_1;
        hash ^= v1;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v2 *= NUMBER64_2;
        v2 = shifting_hash(v2, 31);
        v2 *= NUMBER64_1;
        hash ^= v2;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v3 *= NUMBER64_2;
        v3 = shifting_hash(v3, 31);
        v3 *= NUMBER64_1;
        hash ^= v3;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v4 *= NUMBER64_2;
        v4 = shifting_hash(v4, 31);
        v4 *= NUMBER64_1;
        hash ^= v4;
        hash = hash * NUMBER64_1 + NUMBER64_4;
    }
    else
    {
        hash = seed + NUMBER64_5;
    }

    hash += (uint64_t)length;

    while (p + 8 <= end)
    {
        uint64_t k1 = hash_get64bits(p);
        k1 *= NUMBER64_2;
        k1 = shifting_hash(k1, 31);
        k1 *= NUMBER64_1;
        hash ^= k1;
        hash = shifting_hash(hash, 27) * NUMBER64_1 + NUMBER64_4;
        p += 8;
    }

    if (p + 4 <= end)
    {
        hash ^= (uint64_t)(hash_get32bits(p)) * NUMBER64_1;
        hash = shifting_hash(hash, 23) * NUMBER64_2 + NUMBER64_3;
        p += 4;
    }

    while (p < end)
    {
        hash ^= (*p) * NUMBER64_5;
        hash = shifting_hash(hash, 11) * NUMBER64_1;
        p++;
    }

    hash ^= hash >> 33;
    hash *= NUMBER64_2;
    hash ^= hash >> 29;
    hash *= NUMBER64_3;
    hash ^= hash >> 32;

    return hash;
}

uint64_t hash(const void *data, uint64_t length, uint64_t seed)
{
    if ((((uint64_t)data) & 7) == 0)
    {
        return string_key_hash_computation(data, length, seed, 1);
    }
    return string_key_hash_computation(data, length, seed, 0);
}


/*
欣欣：

运算符 含义  描述
<<  左移  用来将一个数的各二进制位全部左移N位，高位舍弃，低位补0。
>>  右移  将一个数的各二进制位右移N位，移到右端的低位被舍弃，对于无符号数，高位补0。
&   按位与 如果两个相应的二进制位都为1，则该位的结果值为1，否则为0。
l   按位或 两个相应的二进制位中只要有一个为1，该位的结果值为1，否则为0。

若想取data的二进制的后面3位数，那么可以找一个后3位是1其余位是0的数7，（7转换为二进制为00000111)，data&7就得到了data的后三位。
        
*/



/*
欣欣从hash.h 拷贝：
Function: hash() 
        This function is used to compute the hash value of a string key;
        For integer keys, two different and independent hash functions should be used. 
        For example, Jenkins Hash is used for the first hash funciton, and murmur3 hash is used for
        the second hash funciton.

*/