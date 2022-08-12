#include "continuity_hashing.h"

/*
Function: HASH()
        Compute the hash value of a key-value item
*/
uint64_t HASH(continuity_hash *continuity, const uint8_t *key) {
    return (hash((void *)key, strlen(key), continuity->seed));
}

/*
欣欣：
    continuity_hash.h 中写有#include "hash.h"
    且hash.h里有函数声明：uint64_t hash(const void *data, uint64_t length, uint64_t seed);
    （这个const void *data，应该是指 通过data指针不能修改 原本指向的数据的值。）
*/



/*
Function: IDX() 
        Compute the hash location
    /是除，如果是整数相除，只取整数部分，没有四舍五入之类的。
    %是取模，即取除法的余数。
    capacity是有编号的桶的数量。
*/
uint64_t IDX(uint64_t hashKey, uint64_t capacity) {
    return hashKey % capacity;
}




/*
欣欣：
https://blog.csdn.net/unix21/article/details/12893171

对齐
数据的对齐(alignment)是指数据的地址和由硬件条件决定的内存块大小之间的关系。一个变量的地址是它大小的倍数的时候，这就叫做自然对齐(naturally aligned)。例如，对于一个32bit的变量，如果它的地址是4的倍数，-- 就是说，如果地址的低两位是0，那么这就是自然对齐了。所以，如果一个类型的大小是2n个字节，那么它的地址中，至少低n位是0。对齐的规则是由硬件引起的。一些体系的计算机在数据对齐这方面有着很严格的要求。在一些系统上，一个不对齐的数据的载入可能会引起进程的陷入。在另外一些系统，对不对齐的数据的访问是安全的，但却会引起性能的下降。在编写可移植的代码的时候，对齐的问题是必须避免的，所有的类型都该自然对齐。

预对齐内存的分配
在大多数情况下，编译器和C库透明地帮你处理对齐问题。POSIX 标明了通过malloc( ), calloc( ), 和 realloc( ) 返回的地址对于任何的C类型来说都是对齐的。在Linux中，这些函数返回的地址在32位系统是以8字节为边界对齐，在64位系统是以16字节为边界对齐的。有时候，对于更大的边界，例如页面，程序员需要动态的对齐。虽然动机是多种多样的，但最常见的是直接块I/O的缓存的对齐或者其它的软件对硬件的交互，因此，POSIX 1003.1d提供一个叫做posix_memalign( )的函数：

#include <stdlib.h>
int posix_memalign (void **memptr, size_t alignment, size_t size);* 
调用posix_memalign( )成功时会返回size字节的动态内存，并且这块内存的地址是alignment的倍数。参数alignment必须是2的幂，还是void指针的大小的倍数。返回的内存块的地址放在了memptr里面，函数返回值是0.

调用失败时，没有内存会被分配，memptr的值没有被定义，返回如下错误码之一：
EINVAL
参数不是2的幂，或者不是void指针的倍数。
ENOMEM
没有足够的内存去满足函数的请求。
要注意的是，对于这个函数，errno不会被设置，只能通过返回值得到。

由posix_memalign( )获得的内存通过free( )释放。用法很简单：

char *buf;
int ret;    
// allocate 1 KB along a 256-byte boundary 
ret = posix_memalign (&buf, 256, 1024);
if (ret) { fprintf (stderr, "posix_memalign: %s\n", strerror (ret)); return -1;}  // use 'buf'... 
free (buf);


fprintf是C/C++中的一个格式化库函数，位于头文件<cstdio>中，其作用是格式化输出到一个流文件中；函数原型为int fprintf( FILE *stream, const char *format, [ argument ]...)，fprintf()函数根据指定的格式(format)，向输出流(stream)写入数据(argument)。
stderr: 【unix】标准输出(设备)文件，对应终端的屏幕。
strerror: 通过标准错误的标号，获得错误的描述字符串 ，将单纯的错误标号转为字符串描述，方便用户查找错误。

欣欣：那下边就是allocate size 字节 along a 64-byte boundary 。。64字节对齐，是不是为了cacheline呀？或者为了"一个变量的地址是它大小的倍数的时候，这就叫做自然对齐" ？
*/

void* alignedmalloc(size_t size) {
  void* ret;
  posix_memalign(&ret, 64, size);
  return ret;
}

/*
Function: generate_seed() 
        Generate a randomized seed for a hash function

欣欣：
srand(time(NULL)); 指令意思： srand函数是随机数发生器的初始化函数。
　　原型：void srand(unsigned seed);
　　用法：它需要提供一个种子，这个种子会对应一个随机数，如果使用相同的种子后面的rand()函数会出现一样的随机数。如： srand(1); 直接使用1来初始化种子。不过为了防止随机数每次重复，常常使用系统时间来初始化，即使用 time函数来获得系统时间，它的返回值为从 00:00:00 GMT, January 1, 1970 到现在所持续的秒数，然后将time_t型数据（其实就是一个大整数）转化为(unsigned)型再传给srand函数，即： srand((unsigned) time(&t)); 还有一个经常用法，不需要定义time_t型t变量,即： srand((unsigned) time(NULL)); 直接传入一个空指针，因为你的程序中往往并不需要经过参数获得的t数据。
    那么怎么利用rand产生真正的“随机数”呢？ 答案是通过srand（time（NULL））使得随机数种子随时间的变化而变化。
*/
void generate_seed(continuity_hash *continuity)
{
    srand(time(NULL));
    continuity->seed = rand();
   //下边这行应该没必要
   // continuity->seed = continuity->seed << (rand() % 63); //欣：rand()%63指一个在0~62中的随机数
}

/*
Function: continuity_init() 
        Initialize a level continuity hash table
C 标准库 - <math.h>
C 库函数 double pow(double x, double y) 返回 x 的 y 次幂

在continuity_hash 结构体里，continuity_size = log2(segment_num)

*/
continuity_hash *continuity_init(uint64_t continuity_size)
{
    continuity_hash *continuity = alignedmalloc(sizeof(continuity_hash));
    if (!continuity)
    {
        printf("The continuity hash table initialization fails:1\n");
        exit(1);
    }

    // continuity_size = log2(segment_num).
    continuity->continuity_size = continuity_size;
    continuity->segment_num = pow(2, continuity_size);
    generate_seed(continuity);
    continuity->segments = alignedmalloc(pow(2, continuity_size)*sizeof(continuity_segment)); //等号左边应该没有问题吧 尴尬
    continuity->continuity_item_num = 0;
    continuity->continuity_expand_time = 0;
    continuity->resize_state = 0;
    continuity->interim_continuity_segments = NULL;  //持久化+++

    if (!continuity->segments)
    {
        printf("The continuity hash table initialization fails:2\n");
        exit(1);
    }

    uint64_t i;
    for (i = 0; i < continuity->segment_num; i++) {
        continuity->segments[i].bucket[1].indicators = 0;
    }

    /*
    //持久化+++  参考了level的有问题的代码。因为continuity本身就是指针，本身就是程序数据在内存中的地址。所以不应该是&continuity而应该是continuity，且&ptr应该是ptr。
    //https://blog.csdn.net/Meditator_hkx/article/details/79658197?utm_source=blogxgwz5
    //变量的地址，在C语言中，一般写作指针类型。不同类型的变量地址，用不同的指针进行保存。比如，char 类型的地址，使用char*保存，而int型地址，用int *保存。64位编译器，地址占64位，8字节，可以使用long保存。不过不推荐使用整型类型保存地址，会带来移植上的不通用。
    uint64_t *ptr = (uint64_t *)&continuity;
    for(; ptr < (uint64_t *)&continuity + sizeof(continuity_hash); ptr = ptr + 64)
        pflush((uint64_t *)&ptr);
    */

    
    //持久化+++    我觉得是对的，也经过了陈章玉确认
    uint64_t *ptr = (uint64_t *)continuity;
    for(; ptr < (uint64_t *)continuity + sizeof(continuity_hash); ptr = ptr + 64)
        pflush((uint64_t *)ptr);

    printf("Continuity hashing: ASSOC_NUM %d, SHARED_BUCKETS_NUM %d, KEY_LEN %d, VALUE_LEN %d \n", ASSOC_NUM, SHARED_BUCKETS_NUM, KEY_LEN, VALUE_LEN);
    printf("The number of all segments: %ld\n", continuity->segment_num);
    printf("The number of all buckets: %ld\n", continuity->segment_num * (SHARED_BUCKETS_NUM + 2));
    printf("The number of all entries: %ld\n", continuity->segment_num * (SHARED_BUCKETS_NUM + 2) * ASSOC_NUM);
    printf("The continuity hash table initialization succeeds!\n");
    return continuity;
}





/*
Function: continuity_expand()
        Expand a continuity hash table, resizing为当前的4倍容量      
*/
void continuity_expand(continuity_hash *continuity) 
{
    if (!continuity)
    {
        printf("The expanding fails: 1\n");
        exit(1);
    }
    continuity->resize_state = 1;
    pflush((uint64_t *)&continuity->resize_state); //持久化+++


    // continuity_size = log2(segment_num). 
    continuity->segment_num = pow(2, continuity->continuity_size + 2);
    //continuity_segment *newSegments = alignedmalloc(continuity->segment_num * sizeof(continuity_segment));
    continuity->interim_continuity_segments = alignedmalloc(continuity->segment_num * sizeof(continuity_segment)); //持久化+++
    

    if (!continuity->interim_continuity_segments) {  //持久化+++
        printf("The expanding fails: 2\n");
        exit(1);
    }
    pflush((uint64_t *)&continuity->interim_continuity_segments); //持久化+++

    uint64_t new_continuity_item_num = 0;
    uint64_t old_idx;
    //continuity_size = log2(segment_num). 
    for (old_idx = 0; old_idx < pow(2, continuity->continuity_size); old_idx ++){   //针对每一个大segment
        uint64_t i, j, k, m, n1, n2, n3; 
      for(k = 0; k < (SHARED_BUCKETS_NUM + 2); k ++){  //k 针对每一个大segment里的每一个bucket

        for(i = 0; i < ASSOC_NUM; i ++){  //i 针对每一个bucket里的每一个slot
        
            n1 = k * ASSOC_NUM + i + 1;
        	uint1_t nt1 = ( (continuity->segments[old_idx].bucket[1].indicators) >> (32-n1) )&1;  //欣欣：得到从左到右第n1位 (1~32)
            if (nt1 == 1)
            {
                uint8_t *key = continuity->segments[old_idx].bucket[k].slot[i].key;
                uint8_t *value = continuity->segments[old_idx].bucket[k].slot[i].value;

                uint64_t idx = IDX(HASH(continuity, key), continuity->segment_num *2);  //有编号的哈希桶的编号
                //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
                //欣欣：如果a 和 b 都是整数，a / b 的结果也是整数，如果不能整除，那么就直接丢掉小数部分，只保留整数部分。

                uint8_t insertSuccess = 0;
         
                if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后写共享桶
                {
                	ok:
                	for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
                	{
                		for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
                		{
                			n2 = m * ASSOC_NUM + j + 1;
                            //持久化+++  把 newSegments 换成 continuity->interim_continuity_segments
                			//uint1_t nt2 = ( (newSegments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                            uint1_t nt2 = ( (continuity->interim_continuity_segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                            
                			if ( nt2 == 0)
                			{
                				// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                				memcpy(continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].key, key, KEY_LEN);
                                memcpy(continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].value, value, VALUE_LEN);
                                
                                //欣欣：给indicators的相应位 赋1 。应该无误。
                                continuity->interim_continuity_segments[idx/2].bucket[1].indicators = (1 << (32-n2)) | (continuity->interim_continuity_segments[idx/2].bucket[1].indicators);

                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].key);
                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].value);
                                asm_mfence();
                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[1].indicators);
                                asm_mfence();
                                
                                insertSuccess = 1;
                                new_continuity_item_num ++;
                                goto HERE; //跳出双层循环。应该无误，待确定。
                			}
                		}
                	}
                }
                else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前写共享桶 （桶是倒序，桶内是顺序）
                {
                	//Here:
                	for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
                	{
                		for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
                		{
                			n3 = m * ASSOC_NUM + j + 1;
                			uint1_t nt3 = ( (continuity->interim_continuity_segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                			if ( nt3 == 0)
                			{
                				// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                				memcpy(continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].key, key, KEY_LEN);
                                memcpy(continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].value, value, VALUE_LEN);
                                
                                //欣欣：给indicators的相应位 赋1 。应该无误。
                                continuity->interim_continuity_segments[idx/2].bucket[1].indicators = (1 << (32-n3)) | (continuity->interim_continuity_segments[idx/2].bucket[1].indicators);
                                
                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].key);
                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[m].slot[j].value);
                                asm_mfence();
                                pflush((uint64_t *)&continuity->interim_continuity_segments[idx/2].bucket[1].indicators);
                                asm_mfence();

                                insertSuccess = 1;
                                new_continuity_item_num ++;
                                goto HERE; //跳出双层循环。应该无误，待确定。
                			}
                		}
                	}
                }
HERE:
                if(!insertSuccess){
                    printf("The expanding fails: 3\n");
                    exit(1);                    
                }

                //欣欣：给相应的indicators的从左到右第n1位 (1~32) 赋0
                continuity->segments[old_idx].bucket[1].indicators = (~(1 << (32-n1))) & (continuity->segments[old_idx].bucket[1].indicators);
                pflush((uint64_t *)&continuity->segments[old_idx].bucket[1].indicators);
                asm_mfence();
            }
        }
      }
    }

    continuity->continuity_size += 2;

    free(continuity->segments);
    continuity->segments = continuity->interim_continuity_segments;
    continuity->interim_continuity_segments = NULL;
    
    continuity->continuity_item_num = new_continuity_item_num;
    continuity->continuity_expand_time ++;

    uint64_t *ptr = (uint64_t *)continuity;
    for(; ptr < (uint64_t *)continuity + sizeof(continuity_hash); ptr = ptr + 64)
        pflush((uint64_t *)ptr);


    continuity->resize_state = 0;
    pflush((uint64_t *)&continuity->resize_state);

}





/*
Function: continuity_shrink()   
        收缩。 Shrink a continuity hash table; resizing为当前的 1/4 容量
*/
void continuity_shrink(continuity_hash *continuity)
{
    if (!continuity)
    {
        printf("The shrinking fails: 1\n");
        exit(1);
    }

    // The shrinking is performed only when the hash table has very few items.
    if(continuity->continuity_item_num > continuity->segment_num * (SHARED_BUCKETS_NUM + 2) * ASSOC_NUM * 0.4){
        printf("The shrinking fails: 2\n");
        exit(1);
    }

    continuity->resize_state = 2;
    pflush((uint64_t *)&continuity->resize_state);

    continuity->continuity_size -= 2;

    // continuity_size = log2(segment_num)
    continuity_segment *newSegments = alignedmalloc( pow(2, continuity->continuity_size) * sizeof(continuity_segment));
    
    if (!newSegments) {
        printf("The shrinking fails: 4\n");
        exit(1);
    }


    //continuity_segment *imSegments = continuity->segments;   //把imSegments指向原大哈希表 
    continuity->interim_continuity_segments = continuity->segments;   //持久化+++ 
    continuity->segments = newSegments;
    newSegments = NULL; //指针要小心

    //这里直接舍弃了原较大哈希表的数目。其实也许有风险。。
    continuity->continuity_item_num = 0;
    continuity->segment_num = pow(2, continuity->continuity_size);
    continuity->continuity_expand_time = 0;
    // continuity_expand_time用于Indicate whether the hash table was expanded, ">1 or =1": Yes, "0": No
    // 其实continuity_expand_time 初始是0，扩容后就会continuity_expand_time ++; 此时收缩后，很可能是continuity_expand_time --更合适吧？


    uint64_t *ptr = (uint64_t *)continuity;
    for(; ptr < (uint64_t *)continuity + sizeof(continuity_hash); ptr = ptr + 64)
        pflush((uint64_t *)ptr);


    //continuity_size = log2(segment_num). 
    uint64_t old_idx, k, i, n;
    for (old_idx = 0; old_idx < pow(2, continuity->continuity_size+2); old_idx ++) { //依次扫描原大哈希表的每一个大segment
    	for(k = 0; k < (SHARED_BUCKETS_NUM + 2); k ++){  //k 依次扫描原大哈希表某大segment的各个bucket
    		for(i = 0; i < ASSOC_NUM; i ++){  //i 依次扫描原大哈希表某桶的各个slot
    			
    			n = k * ASSOC_NUM + i + 1;
    			uint1_t nt = ( (continuity->interim_continuity_segments[old_idx].bucket[1].indicators) >> (32-n) )&1;  //欣欣：得到从左到右第n位 (1~32)
    			if (nt == 1)
    			{
    				if(continuity_insert(continuity, continuity->interim_continuity_segments[old_idx].bucket[k].slot[i].key, continuity->interim_continuity_segments[old_idx].bucket[k].slot[i].value)){ //这个函数插入成功就是0，不成功是1
    					printf("The shrinking fails: 3\n");
    					exit(1);
    				}

    				//欣欣：插入成功后，给相应的indicators的从左到右第n位 (1~32) 赋0
    				continuity->interim_continuity_segments[old_idx].bucket[1].indicators = (~(1 << (32-n))) & (continuity->interim_continuity_segments[old_idx].bucket[1].indicators);
                    pflush((uint64_t *)&continuity->interim_continuity_segments[old_idx].bucket[1].indicators);

    			}
    		}
    	}
    }     
 
    free(continuity->interim_continuity_segments); 
    //欣欣相比level的持久化新增：在释放动态分配的内存后，把指向该内存的指针设置为0，是个好习惯。level里忘记这样了。。
    continuity->interim_continuity_segments = NULL;

    continuity->resize_state = 0;
    pflush((uint64_t *)&continuity->resize_state);
}





/*
Function: continuity_insert() 
        Insert a key-value item into continuity hash table;
        这个函数插入成功返回0，不成功返回1
*/
uint8_t continuity_insert(continuity_hash *continuity, uint8_t *key, uint8_t *value)
{
    while (continuity->resize_state != 0) ;
    uint64_t hash = HASH(continuity, key);
    uint64_t idx = IDX(hash, continuity->segment_num * 2);

    uint64_t m, j, n2, n3;

    //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
    if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后写共享桶
    {

        for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n2 = m * ASSOC_NUM + j + 1;
                uint1_t nt2 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                if ( nt2 == 0)
                {
                	// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                	memcpy(continuity->segments[idx/2].bucket[m].slot[j].key, key, KEY_LEN);
                    memcpy(continuity->segments[idx/2].bucket[m].slot[j].value, value, VALUE_LEN);
                                
                    //欣欣：给indicators的相应位 赋1 。应该无误。
                    continuity->segments[idx/2].bucket[1].indicators = (1 << (32-n2)) | (continuity->segments[idx/2].bucket[1].indicators);

                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[m].slot[j].key);
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[m].slot[j].value);
                    asm_mfence();
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);

                    continuity->continuity_item_num ++;
                    asm_mfence();
                    return 0;
                }
            }
        }
    }
    else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前写共享桶 （桶是倒序，桶内是顺序）
    {

        for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n3 = m * ASSOC_NUM + j + 1;
                uint1_t nt3 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                if ( nt3 == 0)
                {
                	// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                	memcpy(continuity->segments[idx/2].bucket[m].slot[j].key, key, KEY_LEN);
                    memcpy(continuity->segments[idx/2].bucket[m].slot[j].value, value, VALUE_LEN);
                                
                    //欣欣：给indicators的相应位 赋1 。应该无误。
                    continuity->segments[idx/2].bucket[1].indicators = (1 << (32-n3)) | (continuity->segments[idx/2].bucket[1].indicators);

                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[m].slot[j].key);
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[m].slot[j].value);
                    asm_mfence();
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);

                    continuity->continuity_item_num ++;
                    asm_mfence();
                    return 0;
                }
            }
        }
    }

    //欣欣：即使上述没空位没办法插入，我们暂时选择不移动。
    return 1;
}



/*
Function: continuity_query() 
        Lookup a key-value item in continuity hash table.
        应落在桶0（左桶）的数据，从前往后找共享桶。
        应落在桶1（右桶）的数据，从后往前找共享桶 （桶是倒序，桶内是顺序）。
*/
uint8_t* continuity_query(continuity_hash *continuity, uint8_t *key)
{
    uint64_t hash = HASH(continuity, key);
    uint64_t idx = IDX(hash, continuity->segment_num * 2);

    uint64_t m, j, n2, n3;

    //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
    if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后找共享桶
    {

        for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n2 = m * ASSOC_NUM + j + 1;
                uint1_t nt2 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                if (nt2 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                    return continuity->segments[idx/2].bucket[m].slot[j].value;
                }
            }
        }
    }
    else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前找共享桶 （桶是倒序，桶内是顺序）
    {

        for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n3 = m * ASSOC_NUM + j + 1;
                uint1_t nt3 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                if (nt3 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                	return continuity->segments[idx/2].bucket[m].slot[j].value;
                }
            }
        }
    }    

    return NULL;  //注意
}



/*
Function: continuity_delete() 
        Remove a key-value item from continuity hash table;
*/
uint8_t continuity_delete(continuity_hash *continuity, uint8_t *key)
{
    uint64_t hash = HASH(continuity, key);
    uint64_t idx = IDX(hash, continuity->segment_num * 2);

    uint64_t m, j, n2, n3;

    //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
    if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后找共享桶
    {

        for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n2 = m * ASSOC_NUM + j + 1;
                uint1_t nt2 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                if (nt2 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                    //欣欣：给相应的indicators的从左到右第n2位 (1~32) 赋0
    				continuity->segments[idx/2].bucket[1].indicators = (~(1 << (32-n2))) & (continuity->segments[idx/2].bucket[1].indicators);
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);
    				continuity->continuity_item_num --;
                    asm_mfence();
                    return 0;
                }
            }
        }
    }
    else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前找共享桶 （桶是倒序，桶内是顺序）
    {

        for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n3 = m * ASSOC_NUM + j + 1;
                uint1_t nt3 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                if (nt3 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                	//欣欣：给相应的indicators的从左到右第n3位 (1~32) 赋0
    				continuity->segments[idx/2].bucket[1].indicators = (~(1 << (32-n3))) & (continuity->segments[idx/2].bucket[1].indicators);
                    pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);
    				continuity->continuity_item_num --;
                    asm_mfence();
                    return 0;
                }
            }
        }
    }    

    return 1;
}





/*
Function: continuity_update() 
        Update the value of a key-value item in continuity hash table;
        这是DRAM上的更新模式。
*/
/*
uint8_t continuity_update(continuity_hash *continuity, uint8_t *key, uint8_t *new_value)
{
    uint64_t hash = HASH(continuity, key);
    uint64_t idx = IDX(hash, continuity->segment_num * 2);

    uint64_t m, j, n2, n3;

    //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
    if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后找共享桶
    {

        for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n2 = m * ASSOC_NUM + j + 1;
                uint1_t nt2 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                if (nt2 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                    memcpy(continuity->segments[idx/2].bucket[m].slot[j].value, new_value, VALUE_LEN);
                    return 0;
                }
            }
        }
    }
    else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前找共享桶 （桶是倒序，桶内是顺序）
    {

        for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n3 = m * ASSOC_NUM + j + 1;
                uint1_t nt3 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                if (nt3 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                	memcpy(continuity->segments[idx/2].bucket[m].slot[j].value, new_value, VALUE_LEN);
                    return 0;
                }
            }
        }
    }    


    return 1;
}


*/



/*
Function: continuity_update() 
        Update the value of a key-value item in continuity hash table;
        这是实现成PM上的更新模式。
*/

uint8_t continuity_update(continuity_hash *continuity, uint8_t *key, uint8_t *new_value)
{
    uint64_t hash = HASH(continuity, key);
    uint64_t idx = IDX(hash, continuity->segment_num * 2);

    uint64_t m, j, n2, n3;
    uint64_t k, l, n4, n5;

    //欣欣：bucket idx无论是奇数还是偶数，都是segment   idx/2
    if (idx % 2 == 0) //segemnt中的左桶。应落在桶0（左桶）的数据，从前往后找共享桶
    {
        for(m = 0; m < (SHARED_BUCKETS_NUM + 1); m ++)  //m 针对RDMA读取范围内的每一个bucket
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n2 = m * ASSOC_NUM + j + 1;
                uint1_t nt2 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n2) )&1;  //得到从左到右第n2位 (1~32)
                if (nt2 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                    for(l = 0; l < (SHARED_BUCKETS_NUM + 1); l ++ ) {  //l 针对RDMA读取范围内的每一个bucket
                    	for(k = 0; k < ASSOC_NUM; k ++) {  //k 针对每一个bucket里的每一个slot
                    		n4 = l * ASSOC_NUM + k + 1;
                            uint1_t nt4 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n4) )&1;  //得到从左到右第n4位 (1~32)
                            if (nt4 == 0){  // Log-free update
                            	// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                	            memcpy(continuity->segments[idx/2].bucket[l].slot[k].key, key, KEY_LEN);
                            	memcpy(continuity->segments[idx/2].bucket[l].slot[k].value, new_value, VALUE_LEN);

                            	//欣欣：给相应的indicators的从左到右第n2位 (1~32) 赋0
    				            continuity->segments[idx/2].bucket[1].indicators = (~(1 << (32-n2))) & (continuity->segments[idx/2].bucket[1].indicators);
                                //欣欣：给indicators的从左到右第n4位 赋1 。应该无误。
                                continuity->segments[idx/2].bucket[1].indicators = (1 << (32-n4)) | (continuity->segments[idx/2].bucket[1].indicators);
                               
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[l].slot[k].key);
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[l].slot[k].value);
                             
                                asm_mfence();
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);
                                //疑问：level里如下。token[j]是旧，被赋值0；token[k]是新，被赋值1。这里不是应该同时刷下去这两个吗？不过可能是因为一次flush就flush了一个cacheline or 当前的8字节（flush的时候是一个cacheline），所以这里刷token[j]已经把一整行（包括k）刷下去了。
                                //pflush((uint64_t *)&level->buckets[i][f_idx].token[j]);                               
                                asm_mfence();                
                                return 0;
                            }                  		
                    	}
                    }
                    //没有空位可以更新的时候，就要扩容了
                    return 1;          
                }
            }
        }
    }
    else //segemnt中的右桶。应落在桶1（右桶）的数据，从后往前找共享桶 （桶是倒序，桶内是顺序）
    {
        for(m = SHARED_BUCKETS_NUM + 1; m > 0; m --)  //m 针对RDMA读取范围内的每一个bucket 倒序
        {
            for(j = 0; j < ASSOC_NUM; j ++) //j 针对每一个bucket里的每一个slot
            {
                n3 = m * ASSOC_NUM + j + 1;
                uint1_t nt3 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n3) )&1;  //得到从左到右第n3位 (1~32)
                if (nt3 == 1 && strcmp(continuity->segments[idx/2].bucket[m].slot[j].key, key) == 0)
                {
                	for(l = SHARED_BUCKETS_NUM + 1; l > 0; l --) {  //l 针对RDMA读取范围内的每一个bucket  倒序
                    	for(k = 0; k < ASSOC_NUM; k ++) {  //k 针对每一个bucket里的每一个slot
                    		n5 = l * ASSOC_NUM + k + 1;
                            uint1_t nt5 = ( (continuity->segments[idx/2].bucket[1].indicators) >> (32-n5) )&1;  //得到从左到右第n5位 (1~32)
                            if (nt5 == 0){  // Log-free update
                            	// memcpy指的是C和C++使用的内存拷贝函数，函数原型为void *memcpy(void *destin, void *source, unsigned n)；函数的功能是从源内存地址的起始位置开始拷贝若干个字节到目标内存地址中，即从源source中拷贝n个字节到目标destin中。                                
                	            memcpy(continuity->segments[idx/2].bucket[l].slot[k].key, key, KEY_LEN);
                            	memcpy(continuity->segments[idx/2].bucket[l].slot[k].value, new_value, VALUE_LEN);

                            	//欣欣：给相应的indicators的从左到右第n3位 (1~32) 赋0
    				            continuity->segments[idx/2].bucket[1].indicators = (~(1 << (32-n3))) & (continuity->segments[idx/2].bucket[1].indicators);
                                //欣欣：给indicators的从左到右第n5位 赋1 。应该无误。
                                continuity->segments[idx/2].bucket[1].indicators = (1 << (32-n5)) | (continuity->segments[idx/2].bucket[1].indicators);
                               
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[l].slot[k].key);
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[l].slot[k].value);
                             
                                asm_mfence();
                                pflush((uint64_t *)&continuity->segments[idx/2].bucket[1].indicators);
                                //疑问：level里如下。token[j]是旧，被赋值0；token[k]是新，被赋值1。这里不是应该同时刷下去这两个吗？不过可能是因为一次flush就flush了一个cacheline or 当前的8字节（flush的时候是一个cacheline），所以这里刷token[j]已经把一整行（包括k）刷下去了。
                                //pflush((uint64_t *)&level->buckets[i][f_idx].token[j]);                               
                                asm_mfence();                
                                return 0;
                            }                  		
                    	}
                    }
                    //没有空位可以更新的时候，就要扩容了
                    return 1;     
                }
            }
        }
    }    
    return 1;
}



/*
Function: continuity_destroy() 
        Destroy a continuity hash table
*/
void continuity_destroy(continuity_hash *continuity)
{
    free(continuity->segments);
    continuity = NULL;  //嘻嘻
    //欣欣： 不过初始化时候，有continuity_hash *continuity = alignedmalloc(sizeof(continuity_hash)); 不需要free吗?
}

//欣欣的持久化 也写完了
