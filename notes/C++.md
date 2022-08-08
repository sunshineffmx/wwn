## C++

#### 左值、右值

左值 (lvalue, locator value) 表示了一个占据内存中某个可识别的位置（也就是一个地址）的对象。

左值（lvalue）是一个表达式，它表示一个可被标识的（变量或对象的）内存位置，并且允许使用&操作符来获取这块内存的地址。如果一个表达式不是左值，那它就被定义为右值。

```c++
int i = 42;
i = 43; 
int* p = &i; // ok, i 是左值
int& foo();
foo() = 42; // ok, foo() 是左值
int* p1 = &foo(); // ok, foo() 是左值

int foobar();
int j = 0;
j = foobar(); // ok, foobar() 是右值
int* p2 = &foobar(); // 错误，不能获取右值的地址
j = 42; // ok, 42 是右值
```

**左值引用**

C++中可以使用&符定义引用，如果一个左值同时是引用，就称为“左值引用”，如：

```c++
std::string s;
std::string& sref = s;  //sref为左值引用
```

非const左值引用不能使用右值对其赋值

```c++
std::string& r = std::string();  //错误！std::string（）产生一个临时对象，为右值
```

const左值引用是可以指向右值的

```c++
const int &ref_a = 5; 
```

**右值引用**及其相关的move语义是C++11新引入的最强大的特性之一。前文说到，左值（非const）可以被修改（赋值），但右值不能。但C++11引入的右值引用特性，打破了这个限制，允许我们获取右值的引用，并修改之。

```c++
int &&z3 = x * 6; // 正确，右值引用
int &&ref_a_right = 5; // ok
 
int a = 5;
int &&ref_a_left = a; // 编译不过，右值引用不可以指向左值
 
ref_a_right = 6; // 右值引用的用途：可以修改右值
```

右值引用有办法指向左值吗?使用std::move

```c++
int a = 5; // a是个左值
int &ref_a_left = a; // 左值引用指向左值
int &&ref_a_right = std::move(a); // 通过std::move将左值转化为右值，可以被右值引用指向
 
cout << a; // 打印结果：5
```

https://nettee.github.io/posts/2018/Understanding-lvalues-and-rvalues-in-C-and-C/

https://segmentfault.com/a/1190000003793498

https://zhuanlan.zhihu.com/p/335994370

https://fzheng.me/2019/10/19/std-move-and-rvalue-ref/

https://zhuanlan.zhihu.com/p/50816420

C++11正是通过引入右值引用来优化性能，具体来说是通过移动语义来避免无谓拷贝的问题，通过move语义来将临时生成的左值中的资源无代价的转移到另外一个对象中去，通过完美转发来解决不能按照参数实际类型来转发的问题（同时，完美转发获得的一个好处是可以实现移动语义）

std::move()与std::forward()都仅仅做了**类型转换(可理解为static_cast转换)**而已，真正的移动操作是在移动构造函数或者移动赋值操作符中发生的。

右值引用和std::move被广泛用于在STL和自定义类中**实现移动语义，避免拷贝，从而提升程序性能**。

C++11中有**万能引用**（Universal Reference）的概念：使用`T&&`类型（T是一个被推导类型）的形参既能绑定右值，又能绑定左值。

**引用折叠**：创建引用的引用时（如模板参数、类型别名）会造成引用折叠，所有的折叠引用最终都代表一个引用，要么是左值引用，要么是右值引用。规则是：如果任一引用为左值引用，则结果为左值引用。否则（即两个都是右值引用），结果才是右值引用。

**完美转发**，是指在函数模板中，**std::forward**会将输入的参数原封不动地传递到下一个函数中，这个“原封不动”指的是，如果输入的参数是左值，那么传递给下一个函数的参数的也是左值；如果输入的参数是右值，那么传递给下一个函数的参数的也是右值。

```c++
template<typename T>
void warp(T&& param) {
    func(std::forward<T>(param));
}
```

#### Pair

class pair可将两个value视为一个单元。

struct pair定义于<utility>

```c++
// 操作函数
pair<T1, T2> p					// default构造函数
pair<T1, T2> p(val1, val2)		// 定义并初始化
pair<T1, T2> p(p2)				// copy构造函数
p = p2							// 赋值
p.first; p.second				// 直接成员访问
get<0>(p); get<1>(p)			// 等价于p.first, p.second，始自于C++11
p1 == p2; p1 != p2; p1 < p2; p1 > p2; p1 <= p2; p1 >= p2
p1.swap(p2)						// 交换数据，始自于C++11
swap(p1, p2)					// 交换数据，始自于C++11
make_pair(val1, val2)			// 返回一个pair
```

#### Tuple（不定数的值组）

Tuple是TR1引入的东西，它扩展了pair的概念，拥有任意数量的元素。

#include <tuple>

通过明白的声明，或使用便捷函数make_tuple()，可以创建一个tuple。

通过get<>() function template，可以访问tuple的元素。

```c++
#include <tuple>
#include <iostream>
#include <complex>
#include <string>
using namespace std;

int main()
{
    tuple<string,int,int,complex<double>> t;
    tuple<int,float,string> t1(41,6.3,"nico");
    
    cout << get<0>(t1) << " " << get<1>(t1) << " " << get<2>(t1) << endl;
    
    auto t2 = make_tuple(22, 44, "nico");
    
    get<1>(t1) = get<1>(t2);
}
```

### Smart Pointer

smart pointer帮助管理堆内存，将指针进行封装，可以像普通指针一样进行使用，同时可以自行进行释放，避免忘记释放指针指向的内存地址造成内存泄漏。

**unique_ptr**

实现独占式拥有概念，不能赋值也不能拷贝，保证同一时间该智能指针是其所指向对象的唯一拥有者。

可以使用*和->操作符。

```C++
std::unique_ptr<std::string> up(new std::string("nico"));
(*up)[0] = 'N';
up->append("lai");
std::cout << *up << std::endl;
```

转移拥有权

```c++
std::string* sp = new std::string("hello");
std::unique_ptr<std::string> up1(sp);
std::unique_ptr<std::string> up2(up1);		// error
std::unique_ptr<std::string> up3(std::move(up1));	// ok
std::unique_ptr<std::string> a2(up3.release());
a2.reset()
```

提供的方法：

- get() 获取其保存的原生指针，尽量不要使用
- release() 释放所管理指针的所有权，返回原生指针，但并不销毁原生指针
- reset() 释放并销毁原生指针，如果参数为一个新指针，将管理这个新指针

**shared_ptr**

实现共享式拥有概念，多个智能指针可以指向相同对象，通过引用计数的方式管理指针，每当多一个指针指向该对象时，指向该对象的所有智能指针内部的引用计数加1，每当减少一个智能指针指向对象时，引用计数会减1，当引用计数为 0 时会销毁拥有的原生对象。

可以像使用任何其他pointer一样地使用shared_ptr，可以赋值、拷贝、比较它们，也可以使用操作符*和->访问其所指向的对象。

`shared_ptr`本身拥有的方法主要包括：

- get() 获取其保存的原生指针，尽量不要使用
- reset() 释放所管理的指针，将对象引用计数减一，如果引用计数为0，将销毁释原生指针，如果参数为一个新指针，将管理这个新指针
- unique() 如果引用计数为 1，则返回 true，否则返回 false
- use_count() 返回引用计数的大小

```c++
#include <iostream>
#include <string>
#include <memory>
using namespace std;

shared_ptr<string> pa(new string("juttastd"));
std::shared_ptr<string> pa1 = pa;
shared_ptr<string> pNico = make_shared<string>("nico");
string *origin = pNico.get();
shared_ptr<string> pNico1;
pNico1.reset(new string("nico"));		// 释放并销毁原有对象，持有一个新对象
```

https://blog.csdn.net/u013349653/article/details/51155675

http://bitdewy.github.io/blog/2014/01/12/why-make-shared/

**weak_ptr**

weak_ptr是一个弱指针，允许“共享但不拥有”某对象，可以用于解决shared_ptr的循环引用问题（比如 a 对象持有 b 对象，b 对象持有 a 对象），防止内存泄露。

```c++
std::shared_ptr<A> a1(new A());
std::weak_ptr<A> weak_a1 = a1;//不增加强引用计数
std::shared_ptr<A> shared_a = weak_a1.lock();
weak_a1.reset();//将weak_a1置空
```

class weak_ptr只提供小量操作，只够用来创建、复制、赋值weak pointer，不能使用操作符*和->访问weak_ptr指向的对象，在访问所引用的对象前必须先转换为 std::shared_ptr。

提供的方法：

- expired() 判断所指向的原生指针是否被释放，如果被释放了返回 true，否则返回 false
- use_count() 返回原生指针的引用计数
- lock() 返回 shared_ptr，如果原生指针没有被释放，则返回一个非空的 shared_ptr，否则返回一个空的 shared_ptr
- reset() 将本身置空

#### 指针和引用的区别

* 指针是一个新的变量，指向另一个变量的地址，我们可以通过访问这个地址来修改另一个变量，也可以修改指针所指向的地址使其指向其它变量；而引用是一个别名，对引用的操作就是对变量的本身进行操作，而且其引用的对象在其整个生命周期中是不能被改变的。
* 指针可以有多级，引用只有一级
* 使用指针的话需要解引用才能对对象进行修改，而使用引用可以直接对对象进行修改
* 指针的大小一般是4个字节（sizeof(指针)），引用的大小取决于被引用对象的大小（sizeof(引用)）
* 指针可以为空，引用不可以。

相同点：都是地址的概念。指针指向一块内存，它的内容是所指内存的地址；引用是某块内存的别名。

#### 函数参数传递中值传递、指针传递、引用传递

- 值传递：会为形参重新分配内存空间，将实参的值拷贝给形参，形参的值不会影响实参的值，函数调用结束后形参被释放。
- 指针传递：将实参的地址传递给函数，可以在函数中改变实参的值，调用时为形参指针变量分配内存，实参的地址拷贝到形参指针变量中，结束时释放形参指针变量，可以改变形参指针变量指向的地址。
- 引用传递：形参只是实参的别名，形参的改变会影响实参的值，对于引用参数的改变的处理都会通过一个间接寻址的方式操作到调用函数中的相关变量。

#### 在函数参数传递的时候，什么时候使用指针，什么时候使用引用

- 数组作为参数传递的时候使用指针。
- 对栈空间大小比较敏感（比如递归）的时候使用引用。使用引用传递不需要创建临时变量，开销要更小。
- 类对象作为参数传递的时候使用引用。

#### 堆和栈有什么区别

- 堆是由new和malloc开辟的一块内存，由程序员手动管理，栈是编译器自动管理的内存，存放函数的参数和局部变量。
- 对于堆来讲，频繁的 new/delete 会产生内存碎片，而栈是后进先出的队列，内存是连续的。
- 堆分配时，系统会遍历一个操作系统用于记录内存空闲地址的链表，当找到一个空间大于所申请空间的堆结点后，就会将该结点从链表中删除，并将该结点的内存分配给程序，然后在这块内存区域的首地址处记录分配的大小，这样我们在使用delete来释放内存的时候，delete才能正确地识别并删除该内存区域的所有变量。另外，我们申请的内存空间与堆结点的内存空间不一定相等，这是系统会自动将堆结点上多出来的那部分内存空间放回空闲链表中。而栈分配时，只要栈的剩余空间大于所申请的空间，系统就会为程序提供内存，否则栈溢出。
- 从栈获得的空间较小，一般为1~2M。堆的大小受限于计算机系统中有效的虚拟内存，堆获得的空间比较灵活，也比较大。
- 堆的生长空间向上，地址越来越大，栈的生长空间向下，地址越来越小。

#### 堆快一点还是栈快一点

栈快一点。因为操作系统会在底层对栈提供支持，会分配专门的寄存器存放栈的地址，栈的入栈出栈操作也十分简单，并且有专门的指令执行，所以栈的效率比较高。而堆的操作是由C/C++函数库提供的，在分配堆内存的时候需要一定的算法寻找合适大小的内存。并且获取堆的内容需要两次访问，第一次访问指针，第二次根据指针保存的地址访问内存，因此堆比较慢。

#### 区别以下指针类型

`int *p[10]`表示指针数组，强调数组概念，是一个数组变量，数组大小为10，数组内每个元素都是指向int类型的指针变量

`int (*p)[10]`表示数组指针，强调是指针，只有一个变量，是指针类型，不过指向的是一个int类型的数组，这个数组大小是10。

#### C ++内存管理

在C\+\+中，内存分成5个区，他们分别是堆、栈、全局/静态存储区和常量存储区和代码区。

* 栈，在执行函数时，函数内局部变量的存储单元都可以在栈上创建，函数执行结束时这些存储单元自动被释放。栈内存分配运算内置于处理器的指令集中，效率很高，但是分配的内存容量有限。
* 堆，就是那些由new分配的内存块，他们的释放编译器不去管，由我们的应用程序去控制，一般一个new就要对应一个delete。如果程序员没有释放掉，那么在程序结束后，操作系统会自动回收。
* 全局/静态存储区，内存在程序编译的时候就已经分配好，这块内存在程序的整个运行期间都存在。它主要存放静态数据（局部static变量，全局static变量）、全局变量和常量。
* 常量存储区，这是一块比较特殊的存储区，他们里面存放的是常量字符串，不允许修改。
* 代码区，存放程序的二进制代码

关于这个有很多种说法，有的会增加一个自由存储区，存放malloc分配得到的内存，与堆相似。

#### new和delete是如何实现的，new 与 malloc的异同处

在new一个对象的时候，首先会**调用malloc为对象分配内存空间**，然后调用对象的**构造函数**。delete会调用对象的析构函数，然后调用free回收内存。

new与malloc都会分配内存空间，但是new还会调用对象的构造函数进行初始化，malloc需要给定空间大小，而new只需要对象名。

#### 既然有了malloc/free，C++中为什么还需要new/delete呢

* malloc/free和new/delete都是用来申请内存和回收内存的，new会建立一个对象，而malloc只分配一块内存区域。
* 对于非基本数据类型的对象，创建的时候需要执行构造函数，销毁的时候要执行析构函数。而malloc/free是库函数，是已经编译的代码，所以不能把构造函数和析构函数的功能强加给malloc/free。

#### delete和delete\[\]的区别

* delete只会调用一次析构函数，而delete\[\]会调用每个成员的析构函数

* 用new分配的内存用delete释放，用new\[\]分配的内存用delete\[\]释放

#### 内存泄露的定义，如何检测与避免？

动态分配内存所开辟的空间，在使用完毕后未手动释放，导致一直占据该内存，即为内存泄漏。

造成内存泄漏的几种原因：

1）类的构造函数和析构函数中new和delete没有配套

2）在释放对象数组时没有使用delete\[\]，使用了delete

3）没有将基类的析构函数定义为虚函数，当基类指针指向子类对象时，如果基类的析构函数不是virtual，那么子类的析构函数将不会被调用，子类的资源没有正确释放，因此造成内存泄露

4）没有正确地清除嵌套的对象指针

避免方法：

1. malloc/free要配套 new/delete配套
2. 使用智能指针；
3. 将基类的析构函数设为虚函数；

#### struct和class的区别

* 默认的访问权限。使用struct时，它的成员的访问权限默认是public的，而class的成员默认是private的
* 默认的继承访问权限不同。struct的继承默认是public继承，而class的继承默认是private继承
* class关键字还可以用于定义模板参数，但是struct关键字不能。

####  C++的四种强制转换

四种强制类型转换操作符分别为：static_cast、dynamic_cast、const_cast、reinterpret_cast

* 1）static_cast ：
  用于各种隐式转换。具体的说，就是用户各种基本数据类型之间的转换，比如把int换成char，float换成int等。以及派生类（子类）的指针转换成基类（父类）指针的转换。

  >特性与要点：
  >
  >1. 它没有**运行时类型检查**，所以是有安全隐患的。
  >2. 在派生类指针转换到基类指针时，是没有任何问题的，在基类指针转换到派生类指针的时候，会有安全问题。
  >3. static_cast不能转换const，volatile等属性

* 2）dynamic_cast：
  用于动态类型转换。具体的说，就是在基类指针到派生类指针，或者派生类到基类指针的转换。
  dynamic_cast**能够提供运行时类型检查**，只用于含有虚函数的类。
  dynamic_cast如果不能转换返回NULL。

* 3）const_cast：
  用于**去除const常量属性**，使其可以修改 ，也就是说，原本定义为const的变量在定义后就不能进行修改的，但是使用const_cast操作之后，可以通过这个指针或变量进行修改; 另外还有volatile属性的转换。const_cast强制转换对象必须为指针或引用。

* 4）reinterpret_cast
  几乎什么都可以转，用在任意的指针之间的转换，引用之间的转换，指针和足够大的int型之间的转换，整数到指针的转换等。但是不够安全。

```c++
int a = 10;
int b = 3;
double result = static_cast<double>(a) / static_cast<double>(b);

const int constant = 21;
const int* const_p = &constant;
int* modifier = const_cast<int*>(const_p);
*modifier = 7;

int *a = new int;
double *d = reinterpret_cast<double *>(a);

Sub * sub = new Sub();
Base* sub2base = dynamic_cast<Base*>(sub);
```

#### 一个函数或者可执行文件的生成过程或者编译过程是怎样的

**预处理，编译，汇编，链接**

* 预处理： 执行对预处理命令进行替换等预处理操作
* 编译：代码优化和生成汇编代码
* 汇编：将汇编代码转化为机器语言
* 链接：将目标文件彼此链接起来

编译分为3步，首先对源文件进行预处理，这个过程主要是处理一些#号定义的命令或语句（如宏、#include、预编译指令#ifdef等），生成.i文件；然后进行编译，这个过程主要是进行词法分析、语法分析和语义分析等，生成.s的汇编文件；最后进行汇编，这个过程比较简单，就是将对应的汇编指令翻译成机器指令，生成可重定位的二进制目标文件(.o文件)

#### 动态链接和静态链接

静态链接把所有用到的函数全部链接到可执行目标(exe)文件中

动态链接是只建立一个引用的接口，而真正的代码和数据存放在另外的可执行模块中，在运行时再装入



链接的目的就是将这些文件对应的目标文件链接成一个整体，从而生成可执行文件。

**静态链接**：静态链接是在形成可执行程序前，将汇编生成的.o文件和所需要的库一起链接打包到可执行文件中去，程序运行的时候不再调用其它的库文件。

- 优点：对运行环境依赖小，具有较好的兼容性
- 缺点：生成的程序较大，需要更多的系统资源(所需的所有库都被打包进可执行文件了)，在装入内存中消耗更多时间；一旦库函数有了更新，必须重新编译应用程序

动态链接：动态链接的进行是在程序执行时，在程序编译时仅仅建立与所需库函数之间的关系，并不会被链接到目标代码中，而是在运行时载入，不同应用程序如果调用相同的的库，内存里只有一份共享库的实例，避免了浪费。

- 缺点：依赖动态库，不能独立运行

#### 定义和声明的区别

* 声明是告诉编译器变量的类型和名字，不会为变量分配空间
* 定义就是对这个变量和函数进行内存分配和初始化，需要分配空间，同一个变量可以被声明多次，但是只能被定义一次

#### typdef和define区别

#define是预处理命令，在预处理时执行简单的替换，不做正确性的检查

typedef是在编译时处理的，它是在自己的作用域内给已经存在的类型一个别名

#### define 和const的联系与区别

联系：都是定义常量的一种方法

区别：

* **类型的不同**：define定义的常量没有类型，直接进行替换，可能会有**多个拷贝，占用的内存空间大**，const定义的常量是有类型的，存放在静态存储区，只有一个拷贝，占用的内存空间小。
* **编译器处理不同：**define定义的常量是在预处理阶段进行替换，而const在编译阶段确定它的值。
* **安全检查不同：**define不会进行类型安全检查，会有边际效应的错误，而const会进行类型安全检查，安全性更高。
* **存储方式不同：**define直接进行替换，不单独分配内存，存储于程序段；const需要进行内存分配，存储于代码段。

#### inline关键字说一下 和宏定义有什么区别

inline是内联的意思，可以定义比较小的函数。因为函数频繁调用会占用很多的栈空间，进行入栈出栈操作也耗费计算资源，所以可以用inline关键字修饰频繁调用的小函数。编译器会**在编译阶段将函数体直接嵌入调用处**。

1、内联函数在编译时展开，而宏在预处理阶段展开

2、在编译的时候，内联函数直接被嵌入到目标代码中去，而宏只是一个简单的文本替换。

3、内联函数可以进行诸如类型安全检查、语句是否正确等编译功能，宏不具有这样的功能。

4、宏不是函数，而inline是函数

5、宏在定义时要小心处理宏参数，一般用括号括起来，否则容易出现二义性。而内联函数不会出现二义性。

6、inline可以不展开，宏一定要展开。因为inline指示对编译器来说，只是一个建议，编译器可以选择忽略该建议，不对该函数进行展开。

7、宏定义在形式上类似于一个函数，但在使用它时，仅仅只是做预处理器符号表中的简单替换，因此它不能进行参数有效性的检测，也就不能享受C++编译器严格类型检查的好处，另外它的返回值也不能被强制转换为可转换的合适的类型，这样，它的使用就存在着一系列的隐患和局限性。

关键字 inline 必须与函数定义体放在一起才能使函数成为内联，仅**将 inline 放在函数声明前面不起任何作用**。

对于**类内定义成员函数**，可以不用在函数头部加inline关键字，因为编译器会自动将类内定义的函数声明为内联函数。

```c++
class temp{
public:
    int  amount;
    //构造函数
    temp(int amount){
        this->amount = amount;
    }

    //普通成员函数，在类内定义时前面可以不加inline
    void print_amount(){
        cout << this-> amount;
    }
}
```

内联是以代码膨胀（复制）为代价，**仅仅省去了函数调用的开销**，从而提高函数的执行效率。 
如果执行函数体内代码的时间，相比于函数调用的开销较大，那么效率的收获会很少。另一方面，每一处内联函数的调用都要复制代码，将使程序的总代码量增大，消耗更多的内存空间。

#### 虚函数可以是内联函数吗

- 虚函数可以是内联函数，内联是可以修饰虚函数的，但是当虚函数表现多态性的时候不能内联。
- 内联是在编译器建议编译器内联，而虚函数的多态性在运行期，编译器无法知道运行期调用哪个代码，因此虚函数表现为多态性时（运行期）不可以内联。
- inline virtual 唯一可以内联的时候是：编译器知道所调用的对象是哪个类（如 Base::who()），这只有在编译器具有实际对象而不是对象的指针或引用时才会发生。

#### 函数调用过程

ebp指向栈底，基址指针寄存器

esb指向栈顶，栈指针寄存器

- 参数入栈，将函数参数从右向左依次压入栈中
- 返回地址入栈，将当前指令的下一条指令压入栈中
- 函数跳转，从当前代码区跳转到被调用函数的入口处
- 将调用者的ebp压栈保存（push ebp）
- 调整栈帧（mov ebp esp）
- 执行相关操作
- 恢复调用者的栈帧（pop ebp）
- 跳转到下一指令继续执行

https://leeshine.github.io/2019/03/17/stack-of-fuc-call/

#### const用法

修饰**变量**，说明该变量不可以被改变

修饰**类的成员函数**，表示该函数不会修改类中的数据成员，不会调用其他非const的成员函数

修饰**引用**，指向常量的引用，不允许通过该引用修改所引用的对象

修饰**指针**

- 指向常量的指针，不能用于改变其所指对象的值，但可以修改指向常量的指针所指向的对象。把*放在const之后。

- 常量指针，指针本身是常量，必须初始化，并且初始化后值（存储的地址）不能改变。把*放在关键字之前。

```c++
const double homo = 1.14;
const double* cptr = &homo;	// 指向常量的指针
const double homo2 = 5.14;
cptr = &homo2;
double dval = 3.14;
cptr = &dval;		// 允许一个指向常量的指针指向一个非常量对象，但不能通过该指针修改非常量的值

int num = 0;
int* const cur = &num;	// cur是一个常量指针，一直指向num
*cur = 1;	// 可以修改所指变量的值
```

顶层const表示指针本身是个常量，底层const表示指针所指的对象是常量。

#### static

static的意思是静态的，可以用来修饰变量，函数和类成员。

* 变量：被static修饰的变量就是静态变量，它会在程序运行过程中一直存在，会被放在静态存储区。局部静态变量的作用域在函数体中，全局静态变量的作用域在这个文件里。
* 函数：被static修饰的函数就是静态函数，静态函数**只能在本文件中使用**，不能被其他文件调用，也不会和其他文件中的同名函数冲突。
* 类成员：静态成员变量/静态成员函数，被类的所有对象共用，访问这个静态成员变量/静态成员函数不需要引用对象名，而是通过引用类名来访问。

静态成员函数要访问非静态成员时，要通过对象来引用。局部静态变量在函数调用结束后也不会被回收，会一直在程序内存中，直到该函数再次被调用，它的值还是保持上一次调用结束后的值。

注意和const的区别。const强调值不能被修改，而static强调唯一的拷贝，对所有类的对象都共用。

**虚函数（virtual）能是static的吗？**

不能，因为静态成员函数可以不通过对象来调用，即没有隐藏的this指针；而virtual函数一定要通过对象来调用，即有隐藏的this指针。

#### 深拷贝和浅拷贝的区别（举例说明深拷贝的安全性）

浅拷贝就是将对象的指针进行简单的复制，原对象和副本指向的是相同的资源。

而深拷贝是新开辟一块空间，将原对象的资源复制到新的空间中，并返回该空间的地址。

深拷贝可以避免**重复释放和写冲突**。例如使用浅拷贝的对象进行释放后，对原对象的释放会导致内存泄漏或程序崩溃。

#### 类大小计算

首先要明确一个概念，平时所声明的类只是一种类型定义，它本身是没有大小可言的。 我们这里指的类的大小，其实指的是类的对象所占的大小。因此，如果用sizeof运算符对一个类型名操作，得到的是具有该类型实体的大小。

- 类大小的计算遵循结构体的对齐原则
- 类的大小与普通数据成员有关，与成员函数和静态成员无关。即普通成员函数，静态成员函数，静态数据成员，静态常量数据成员均对类的大小无影响
- 虚函数对类的大小有影响，是因为虚函数表指针带来的影响
- 虚继承对类的大小有影响，是因为虚基表指针带来的影响
- 空类的大小是一个特殊情况，空类的大小为1

**解释说明**
静态数据成员之所以不计算在类的对象大小内，是因为类的静态数据成员被该类所有的对象所共享，并不属于具体哪个对象，静态数据成员定义在内存的全局区。

https://blog.csdn.net/fengxinlinux/article/details/72836199

#### 重载、重写

重载（overloadded）：同一作用域内的几个函数名字相同但形参列表不同（形参类型、数量不同），不能通过返回类型不同来区分重载函数。

重写（覆盖 override）：派生类重写基类的虚函数，基类函数必须有virtual关键字，函数名、参数和返回值都相同。

重定义：不在同一个作用域（分别位于派生类和基类），函数名字相同，返回值可以不同。参数不同，此时，不论有无virtual关键字，基类的函数将被隐藏。参数相同，但是基类函数没有virtual关键字，基类的函数被隐藏。

```c++
class Base
 {
 public:
     virtual void a(int x)    {    cout << "Base::a(int)" << endl;      }
     // overload the Base::a(int) function
     virtual void a(double x) {    cout << "Base::a(double)" << endl;   }
     virtual void b(int x)    {    cout << "Base::b(int)" << endl;      }
     void c(int x)            {    cout << "Base::c(int)" << endl;      }
 };

 class Derived : public Base
 {
 public:
     // redefine the Base::a() function
     void a(complex<double> x)   {    cout << "Derived::a(complex)" << endl;      }
     // override the Base::b(int) function
     void b(int x)               {    cout << "Derived::b(int)" << endl;          }
     // redefine the Base::c() function
     void c(int x)               {    cout << "Derived::c(int)" << endl;          }
 };
```

#### override关键字

指定了子类的这个虚函数是重写的父类的。

```c++
class A
{
    virtual void foo();
}
class B : public A
{
    virtual void foo() override;
}
```

#### final关键字

当不希望某个类被继承，或不希望某个虚函数被重写，可以在类名和虚函数后添加final关键字，添加final关键字后被继承或重写，编译器会报错。

```c++
class Base
{
    virtual void foo();
};
 
class A : public Base
{
    void foo() final; // foo 被override并且是最后一个override，在其子类中不可以重写
};

class B final : A // 指明B是不可以被继承的
{
    void foo() override; // Error: 在A中已经被final了
};
 
class C : B // Error: B is final
{
};
```

#### 类

类是 C++ 的核心特性，通常被称为用户定义的类型。

类用于指定对象的形式，它包含了数据表示法和用于处理数据的方法。类中的数据和方法称为类的成员。函数在一个类中被称为类的成员。

类的基本思想是数据抽象和封装。数据抽象是一种依赖于接口和实现分离的编程技术。类的接口包括用户所能执行的操作；类的实现则包括类的数据成员、负责接口实现的函数以及定义类所需各种私有函数。

封装是把数据和操作数据的函数绑定在一起的一个概念，这样能避免数据受到外界的干扰和误用，从而确保了安全。

#### 介绍C++所有的构造函数

C\+\+中的构造函数主要有三种类型：默认构造函数、重载构造函数和拷贝构造函数

* 默认构造函数是当类没有实现自己的构造函数时，编译器默认提供的一个构造函数。
* 重载构造函数也称为一般构造函数，一个类可以有多个重载构造函数，但是需要参数类型或个数不相同。可以在重载构造函数中自定义类的初始化方式。
* 拷贝构造函数是在发生对象复制的时候调用的。 
* 移动构造函数，类似于拷贝构造函数，不同的是它的参数是一个右值引用，移动构造函数仅仅移动数据成员，不会分配新的内存。

#### 什么情况下会调用拷贝构造函数（三种情况）

* 对象以值传递的方式传入函数参数 

  >如 ` void func(Dog dog){};`

* 对象以值传递的方式从函数返回

  >如 ` Dog func(){ Dog d; return d;}`

* 对象需要通过另外一个对象进行初始化

```c++
Person A;
Person B = A;
Person C(A);
```

#### 移动构造函数

- 我们用对象a初始化对象b，后对象a我们就不在使用了，但是对象a的空间还在呀（在析构之前），既然拷贝构造函数，实际上就是把a对象的内容复制一份到b中，那么为什么我们不能直接使用a的空间呢？这样就避免了新的空间的分配，大大降低了构造的成本。这就是移动构造函数设计的初衷；

- 拷贝构造函数中，对于指针，我们一定要采用深层复制，而移动构造函数中，对于指针，我们采用浅层复制。浅层复制之所以危险，是因为两个指针共同指向一片内存空间，若第一个指针将其释放，另一个指针的指向就不合法了。所以我们只要避免第一个指针释放空间就可以了。避免的方法就是将第一个指针（比如a->value）置为NULL，这样在调用析构函数的时候，由于有判断是否为NULL的语句，所以析构a的时候并不会回收a->value指向的空间；

- 移动构造函数的参数和拷贝构造函数不同，拷贝构造函数的参数是一个左值引用，但是移动构造函数的初值是一个右值引用。意味着，移动构造函数的参数是一个右值或者将亡值的引用。也就是说，只用用一个右值，或者将亡值初始化另一个对象的时候，才会调用移动构造函数。而那个move语句，就是将一个左值变成一个将亡值。

#### 成员初始化列表

成员初始化列表就是在类或者结构体的构造函数中，在参数列表后以冒号开头，逗号进行分隔的一系列初始化字段。如下：

```c++
class A{
int id;
string name;
FaceImage face;
A(int& inputID,string& inputName,FaceImage& inputFace):id(inputID),name(inputName),face(inputFace){} // 成员初始化列表
};
```

调用构造函数时，对象在程序进入构造函数函数体之前被创建。也就是说，调用构造函数的时候，先创建对象，再进入函数体。

因为使用成员初始化列表进行初始化的话，会直接使用**传入参数的拷贝构造函数进行初始化**，省去了一次执行传入参数的默认构造函数的过程，否则会调用一次传入参数的默认构造函数+拷贝赋值运算符。所以使用成员初始化列表效率会高一些。

另外，有三种情况是必须使用成员初始化列表进行初始化的：

* 常量成员的初始化，因为常量成员只能初始化不能赋值
* 引用类型
* 没有默认构造函数的对象必须使用成员初始化列表的方式进行初始化

#### 介绍面向对象的三大特性，并且举例说明每一个。

面向对象的三大特性是：封装，继承和多态。

* 封装隐藏了类的实现细节和成员数据，实现了代码模块化，如类里面的private和public；
* 继承使得子类可以复用父类的成员和方法，实现了代码重用；
* 多态则是“一个接口，多个实现”，通过父类调用子类的成员，实现了接口重用，如父类的指针指向子类的对象。

#### 多态的实现（和下个问题一起回答）

C++ 多态包括编译时多态和运行时多态，编译时多态体现在**函数重载和模板**上，运行时多态体现在**虚函数**上。

* 虚函数：在基类的函数前加上virtual关键字，在派生类中重写该函数，运行时将会根据对象的实际类型来调用相应的函数。如果对象类型是派生类，就调用派生类的函数；如果对象类型是基类，就调用基类的函数.

```c++
class A{
    private:
    int i;
    public:
    A();
    A(int num) :i(num) {};
    virtual void fun1();
    virtual void fun2();

};

class B : public A{
    private:
    int j;
    public:
    B(int num) :j(num){};
    virtual void fun2();// 重写了基类的方法
};

// 为方便解释思想，省略很多代码

A a(1);
B b(2);
A *a1_ptr = &a;
A *a2_ptr = &b;

// 当派生类“重写”了基类的虚方法，调用该方法时
// 程序根据 指针或引用 指向的  “对象的类型”来选择使用哪个方法
a1_ptr->fun2();// call A::fun2();
a2_ptr->fun2();// call B::fun2();
// 否则
// 程序根据“指针或引用的类型”来选择使用哪个方法
a1_ptr->fun1();// call A::fun1();
a2_ptr->fun1();// call A::fun1();
```

当基类指针指向派生类的时候，只能操作派生类从基类中继承过来的数据。指向派生类的指针，因为内存空间比基类长，访问的话会导致内存溢出，所以不允许派生类的指针指向基类。

####  C++虚函数相关（虚函数表，虚函数指针），虚函数的实现原理（热门，重要）

C++的虚函数是实现多态的机制。它是通过虚函数表实现的，虚函数表是每个类中存放虚函数地址的指针数组，虚函数表由编译器自动生成和维护。类的实例在调用函数时会在虚函数表中寻找函数地址进行调用，如果子类覆盖了父类的函数，则子类的虚函数表会指向子类实现的函数地址，否则指向父类的函数地址。一个类的所有实例都共享同一张虚函数表。

详见：[C++虚函数表剖析](https://blog.csdn.net/lihao21/article/details/50688337)

* 如果多重继承（B继承A，C继承B）和多继承（同时继承多个基类）的话，子类的虚函数表长什么样子？
  多重继承的情况下越是祖先的父类的虚函数更靠前，多继承的情况下越是靠近子类名称的类的虚函数在虚函数表中更靠前。详见：https://blog.csdn.net/qq_36359022/article/details/81870219

#### 编译器处理虚函数表应该如何处理

编译器处理虚函数的方法是：
如果类中有虚函数，就将虚函数的地址记录在类的虚函数表中。派生类在继承基类的时候，如果有重写基类的虚函数，就将虚函数表中相应的函数指针设置为派生类的函数地址，否则指向基类的函数地址。
为每个类的实例添加一个虚表指针（vptr），虚表指针指向类的虚函数表。实例在调用虚函数的时候，通过这个虚函数表指针找到类中的虚函数表，找到相应的函数进行调用。

定义一个对象后，构造器为其分配空间，而这个指向虚表的指针始终都是在第一个，也就是说这个指针的地址就是对象的首地址，这样方便多态函数的访问。
PS:对象地址的前四个字节存放虚表地址。

详见：https://blog.csdn.net/iFuMI/article/details/51088091

#### 基类的析构函数一般写成虚函数的原因

首先析构函数可以为虚函数，当析构一个指向子类的父类指针时，编译器可以根据虚函数表寻找到子类的析构函数进行调用，从而正确释放子类对象的资源。

如果析构函数不被声明成虚函数，则编译器实施静态绑定，在删除指向子类的父类指针时，只会调用父类的析构函数而不调用子类析构函数，这样就会造成子类对象析构不完全造成内存泄漏。

#### 构造函数为什么一般不定义为虚函数

1）因为构造一个对象时需要确定对象的类型，而虚函数是在运行时确定其类型的。在构造一个对象时，**由于对象还未构造成功，编译器无法知道对象的实际类型**，是类本身还是类的派生类等等。

2）虚函数的调用需要虚函数表指针，而该指针存放在对象的内存空间中；若构造函数声明为虚函数，那么由于对象还未创建，还没有内存空间，更没有虚函数表地址用来调用虚函数即构造函数了。

#### 构造函数或者析构函数中调用虚函数会怎样

在构造函数中调用虚函数，由于当前对象还没有构造完成，此时调用的虚函数指向的是基类的函数实现方式。

在析构函数中调用虚函数，此时调用的是子类的函数实现方式。

在析构函数中也不要调用虚函数。在析构的时候会首先调用子类的析构函数，析构掉对象中的子类部分，然后再调用基类的析构函数析构基类部分，如果在基类的析构函数里面调用虚函数，会导致其调用已经析构了的子类对象里面的函数，这是非常危险的。

假设一个派生类的对象进行析构，首先调用了派生类的析构，然后在调用基类的析构时，遇到了一个虚函数，这个时候有两种选择：Plan A是编译器调用这个虚函数的基类版本，那么虚函数则失去了运行时调用正确版本的意义；Plan B是编译器调用这个虚函数的派生类版本，但是此时对象的派生类部分已经完成析构，“数据成员就被视为未定义的值”，这个函数调用会导致未知行为。

####  纯虚函数

```c++
virtual void func()=0
```

纯虚函数是只有声明没有实现的虚函数，是对子类的约束，是接口继承

包含纯虚函数的类是抽象类，它不能被实例化，只有实现了这个纯虚函数的子类才能生成对象

使用场景：当这个类本身产生一个实例没有意义的情况下，把这个类的函数实现为纯虚函数，比如动物可以派生出老虎兔子，但是实例化一个动物对象就没有意义。并且可以规定派生的子类必须重写某些函数的情况下可以写成纯虚函数。

#### 静态绑定和动态绑定

绑定(Binding)是指将变量和函数名转换为地址的过程。

[C++中的静态绑定和动态绑定](https://www.cnblogs.com/lizhenghn/p/3657717.html)

- 静态类型：对象在声明时采用的类型，在编译期既已确定；
- 动态类型：通常是指一个指针或引用目前所指对象的类型，是在运行期决定的；

静态绑定也就是将该对象相关的属性或函数绑定为它的静态类型，也就是它在声明的类型，在**编译的时候就确定**。在调用的时候编译器会寻找它声明的类型进行访问。

动态绑定就是将该对象相关的属性或函数绑定为它的动态类型，**具体的属性或函数在运行期确定**，**通常通过虚函数实现动态绑定**。

#### 模板的用法与适用场景实现原理

用template \<typename T\>关键字进行声明，接下来就可以进行模板函数和模板类的编写了

编译器会对函数模板进行**两次编译**：在声明的地方对模板代码本身进行编译，这次**编译只会进行一个语法检查**，并不会生成具体的代码。**在运行时对代码进行参数替换后再进行编译**，生成具体的函数代码。

#### 友元

类可以允许其他类或者函数访问它的非公有成员，方法是令其他类或者函数成为它的友元。

友元声明只能出现在类定义的内部，友元不是类的成员。

友元关系不能被传递，也不能被继承。基类的友元在访问派生类成员时不具有特殊性，派生类的友元也不能随意访问基类的成员。

#### 访问权限说明符

- public的变量和函数在类的内部外部都可以访问。
- protected的变量和函数只能在类的内部和其派生类、友元中访问。
- private修饰的变量和函数只能在类内和友元中访问。

#### 继承权限

- public继承：基类的公有成员和保护成员作为派生类的成员时，都保持原有的状态，而基类的私有成员任然是私有的，不能被这个派生类的子类所访问。
- protected继承：基类的所有公有成员和保护成员都成为派生类的保护成员，并且只能被它的派生类成员函数或友元函数访问，基类的私有成员仍然是私有的。
- private继承：基类的所有公有成员和保护成员都成为派生类的私有成员，并不被它的派生类的子类所访问，基类的成员只能由自己派生类访问，无法再往下继承。

#### C++中新增了string，它与C语言中的 char *有什么区别吗？它是如何实现的？

string继承自basic_string,其实是对char进行了封装，封装的string包含了char数组，容量，长度等等属性。

string可以进行动态扩展，在每次扩展的时候另外申请一块原空间大小两倍的空间（2^n），然后将原字符串拷贝过去，并加上新增的内容。

#### 函数指针

指向函数的指针变量，函数的类型是由其返回的数据类型和其参数列表共同决定的。

`int (*pf)(const int&, const int&);`

上面的pf就是一个函数指针，指向所有返回类型为int，并带有两个const int&参数的函数。

```c++
int test(int a){
    return a;
}
int main(int argc, const char * argv[]){
    // 这里定义了一个参数为int类型的函数，函数的返回值为int类型的指针fp
    int (*fp)(int a);
    // 将test函数的地址赋值给指针fp，完成一个指向函数。该指针指向的函数的参数为int类型
    fp = test;
    // 调用test函数，这里返回的值为2
    cout << fp(2) << endl;
    return 0;
}
```

作用：

- 将函数指针指向不同的具体函数实现，调用不同的函数。
- 作为某个函数的参数，回调函数

#### auto、decltype、decltype(auto)

C++11新标准引入了auto类型说明符，让编译器通过初始值来进行类型推演。从而获得定义变量的类型，所以说 **auto** 定义的变量必须有初始值。

有的时候我们还会遇到这种情况，我们希望从表达式中推断出要定义变量的类型，但却不想用表达式的值去初始化变量。还有可能是函数的返回类型为某表达式的值类型。在这些时候auto显得就无力了，所以C++11又引入了第二种类型说明符**decltype**，它的作用是选择并返回操作数的数据类型。在此过程中，编译器只是分析表达式并得到它的类型，却不进行实际的计算表达式的值。

```c++
int a = 0;
decltype(a) b = 4; // a的类型是int, 所以b的类型也是int
```

decltype(auto)是C++14新增的类型指示符，可以用来声明变量以及指示函数返回类型。在使用时，会将“=”号左边的表达式替换掉auto，再根据decltype的推断规则来确定类型。

```c++
int e = 4;
const int* f = &e; // f是底层const
decltype(auto) j = f;//j的类型是const int* 并且指向的是e

int i;
const int &ci = i;
auto i1 = ci;
decltype(auto) i2 = ci;	// i1 不是引用，i2 是引用。一般来说 decltype(auto) 可以获得更准确的类型，但是也有特例。
```

#### volatile关键字

volatile的意思是“脆弱的”，表明它修饰的变量的值十分容易被改变，所以编译器就不会对这个变量进行优化（CPU的优化是让该变量存放到CPU寄存器而不是内存），进而提供稳定的访问。每次读取volatile的变量时，系统总是会从它所在的内存读取数据，并且将它的值立刻保存。

**多线程下的volatile**  

有些变量是用volatile关键字声明的。当两个线程都要用到某一个变量且该变量的值会被改变时，应该用volatile声明，该关键字的作用是防止优化编译器把变量从内存装入CPU寄存器中。如果变量被装入寄存器，那么两个线程有可能一个使用内存中的变量，一个使用寄存器中的变量，这会造成程序的错误执行。volatile的意思是让编译器每次操作该变量时一定要从内存中真正取出，而不是使用已经存在寄存器中的值。

#### mutable关键字

mutable的中文意思是“可变的，易变的”，跟constant（既C++中的const）是反义词。在C++中，mutable也是为了突破const的限制而设置的。被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中。我们知道，如果类的成员函数不会改变对象的状态，那么这个成员函数一般会声明成const的。但是，有些时候，我们需要在const函数里面修改一些跟类状态无关的数据成员，那么这个函数就应该被mutable来修饰，并且放在函数后后面关键字位置。

被mutable修饰的数据成员在const成员函数中也能被修改。

#### explicit关键字

explicit关键字用来修饰类的构造函数，被修饰的构造函数的类，不能发生相应的隐式类型转换，只能以显式的方式进行类型转换，注意以下几点：

- explicit 关键字只能用于类内部的构造函数声明上
- explicit 关键字作用于单个参数的构造函数
- 被explicit修饰的构造函数的类，不能发生相应的隐式类型转换

https://www.cnblogs.com/rednodel/p/9299251.html

#### default、delete关键字

如果类中只定义了一个有参数的构造函数，默认构造函数编译器就不再生成了。那么在外部创建类时，如果创建无参数的类就会出错。通过default关键字来让编译器生成默认构造函数。

delete关键字可以删除构造函数、赋值运算符函数等。

```c++
class CString
{
public:
    CString() = default;
    //构造函数
    CString(const char* pstr) : _str(pstr){}
    void* operator new() = delete;//这样不允许使用new关键字
    //析构函数
    ~CString(){}
public:
     string _str;
};
```

#### this指针

- this指针是类的指针，指向对象的首地址。
- this 指针是一个隐含于每一个非静态成员函数中的特殊指针。它指向调用该成员函数的那个对象。
- this指针只能在成员函数中使用，在全局函数、静态成员函数中都不能用this。
- this指针只有在成员函数中才有定义，且存储位置会因编译器不同有不同存储位置。

**this指针的用处**

一个对象的this指针并不是对象本身的一部分，不会影响sizeof(对象)的结果。this作用域是在类内部，当在类的**非静态成员函数**中访问类的**非静态成员**的时候（全局函数，静态函数中不能使用this指针），编译器会自动将对象本身的地址作为一个隐含参数传递给函数。也就是说，即使你没有写上this指针，编译器在编译的时候也是加上this的，它作为非静态成员函数的隐含形参，对各成员的访问均通过this进行

**this指针的使用**

一种情况就是，在类的非静态成员函数中返回类对象本身的时候，直接使用 return *this；

另外一种情况是当形参数与成员变量名相同时用于区分，如this->n = n （不能写成n = n）

**this指针是什么时候创建的？**

this在成员函数的开始执行前构造，在成员的执行结束后清除。

**this指针存放在何处？堆、栈、全局变量，还是其他？**

this指针会因编译器不同而有不同的放置位置。可能是栈，也可能是寄存器，甚至全局变量。在汇编级别里面，一个值只会以3种形式出现：立即数、寄存器值和内存变量值。不是存放在寄存器就是存放在内存中，它们并不是和高级语言变量对应的。

**this指针是如何传递类中的函数的？绑定？还是在函数参数的首参数就是this指针？那么，this指针又是如何找到“类实例后函数的”？**

大多数编译器通过ecx（寄数寄存器）寄存器传递this指针。事实上，这也是一个潜规则。一般来说，不同编译器都会遵从一致的传参规则，否则不同编译器产生的obj就无法匹配了。

在call之前，编译器会把对应的对象地址放到eax中。this是通过函数参数的首参来传递的。this指针在调用之前生成，至于“类实例后函数”，没有这个说法。类在实例化时，只分配类中的变量空间，并没有为函数分配空间。自从类的函数定义完成后，它就在那儿，不会跑的。

**我们只有获得一个对象后，才能通过对象使用this指针。如果我们知道一个对象this指针的位置，可以直接使用吗？**

**this指针只有在成员函数中才有定义。**因此，你获得一个对象后，也不能通过对象使用this指针。所以，我们无法知道一个对象的this指针的位置（只有在成员函数里才有this指针的位置）。当然，在成员函数里，你是可以知道this指针的位置的（可以通过&this获得），也可以直接使用它。

#### 虚拟继承

虚拟继承是多重继承中特有的概念。虚拟基类是为解决多重继承而出现的。如:类D继承自类B1、B2，而类B1、B2都继承自类A，因此在类D中两次出现类A中的变量和函数。为了节省内存空间，可以将B1、B2对A的继承定义为虚拟继承，而A就成了虚拟基类。

即便该基类在多条链路上被一个子类继承，但是该子类中只包含一个该虚基类的备份。

```c++
#include<iostream>  
using namespace std;  

class A  //大小为4  
{  
    public:  
    int a;  
};  
class B :virtual public A  //大小为12，变量a,b共8字节，虚基类表指针4  
{  
    public:  
    int b;  
};  
class C :virtual public A //与B一样12  
{  
    public:  
    int c;  
};  
class D :public B, public C //24,变量a,b,c,d共16，B的虚基类指针4，C的虚基类指针4
{  
    public:  
    int d;  
};  
```

虚继承底层实现原理与编译器相关，一般通过虚基类指针和虚基类表实现，每个虚继承的子类都有一个虚基类指针（占用一个指针的存储空间，4字节）和虚基类表（不占用类对象的存储空间）（需要强调的是，虚基类依旧会在子类里面存在拷贝，只是仅仅最多存在一份而已，并不是不在子类里面了）；当虚继承的子类被当做父类继承时，虚基类指针也会被继承。

实际上，vbptr指的是虚基类表指针（virtual base table pointer），该指针指向了一个虚基类表（virtual table），虚表中记录了虚基类与直接继承类的偏移地址；通过偏移地址，这样就找到了虚基类成员，而虚继承也不用像普通多继承那样维持着公共基类（虚基类）的两份同样的拷贝，节省了存储空间。

#### C++ 11有哪些新特性？

- nullptr替代 NULL，nullptr是为了解决原来C\+\+中NULL的二义性问题而引进的一种新的类型，因为NULL实际上代表的是0，而nullptr是void*类型的。
- 引入了 auto 和 decltype 这两个关键字实现了自动类型推导。
- 基于范围的 for 循环for(auto& i : res){}
- Lambda 表达式（匿名函数），用于创建并定义匿名的函数对象，以简化编程工作。
- 右值引用和move语义
- 智能指针
- final关键字来禁止虚函数被重写/禁止类被继承，override来显式地重写虚函数
- …

#### 介绍一下几种典型的锁

**读写锁**

- 多个读者可以同时进行读
- 写者必须互斥（只允许一个写者写，也不能读者写者同时进行）
- 写者优先于读者（一旦有写者，则后续读者必须等待，唤醒时优先考虑写者）

**互斥锁**

一次只能一个线程拥有互斥锁，其他线程只有等待

互斥锁是在抢锁失败的情况下主动放弃CPU进入睡眠状态直到锁的状态改变时再唤醒，而操作系统负责线程调度，为了实现锁的状态发生改变时唤醒阻塞的线程或者进程，需要把锁交给操作系统管理，所以互斥锁在加锁操作时涉及上下文的切换。

互斥锁实际的效率还是可以让人接受的，加锁的时间大概100ns左右，而实际上互斥锁的一种可能的实现是先自旋一段时间，当自旋的时间超过阀值之后再将线程投入睡眠中，因此在并发运算中使用互斥锁（每次占用锁的时间很短）的效果可能不亚于使用自旋锁

```c++
#include <mutex>
std::mutex mylock;
mulock.lock();
mylock.unlock();
```

实现：

```c++
struct mutex {
   atomic_t  count; //引用计数器,1: 所可以利用,小于等于0：该锁已被获取，需要等待
   spinlock_t  wait_lock;//自旋锁类型，保证多cpu下，对等待队列访问是安全的。 
   struct list_head wait_list;//等待队列，如果该锁被获取，任务将挂在此队列上，等待调度。
};
```

**条件变量**

互斥锁一个明显的缺点是他只有两种状态：锁定和非锁定。而条件变量通过允许线程阻塞和等待另一个线程发送信号的方法弥补了互斥锁的不足，他常和互斥锁一起使用，以免出现竞态条件。当条件不满足时，线程往往解开相应的互斥锁并阻塞线程然后等待条件发生变化。

一旦其他的某个线程改变了条件变量，他将通知相应的条件变量唤醒一个或多个正被此条件变量阻塞的线程。总的来说互斥锁是线程间互斥的机制，条件变量则是同步机制。

**自旋锁**

如果进线程无法取得锁，进线程不会立刻放弃CPU时间片，而是一直循环尝试获取锁，直到获取为止。如果别的线程长时期占有锁那么自旋就是在浪费CPU做无用功，但是自旋锁一般应用于加锁时间很短的场景，这个时候效率比较高。

**std::lock_guard**

是一个互斥量的包装类，用来提供自动为互斥量加锁和解锁的功能，简化多线程编程。声明一个局部的std::lock_guard对象，在其构造函数中进行加锁，在其析构函数中进行解锁。最终的结果就是：创建即加锁，作用域结束自动解锁。从而使std::lock_guard()就可以替代lock()与unlock()。但lock_guard本身并没有提供加锁和解锁的接口。

```c++
void safePrint(std::string msg, int val) {
    // 用此语句替换了g_mutex.lock()，参数为互斥锁g_mutex
    std::lock_guard<std::mutex> guard(g_mutex);
    std::cout << msg << val << std::endl;
} // 此时不需要写g_mutex.unlock()，guard出了作用域被释放，自动调用析构函数，于是g_mutex被解锁
```

**std::unique_lock**

std::unique_lock同样能够实现自动加锁与解锁的功能，但比std::lock_guard提供了更多的成员方法，更加灵活一点，相对来说占用空也间更大并且相对较慢，即需要付出更多的时间、性能成本。

```c++
void safePrint(std::string msg, int val) {
    std::mutex my_mutex;
    std::unique_lock<std::mutex> guard(my_mutex);
    // do something 1
    guard.unlock(); // 临时解锁

    // do something 2
    guard.lock(); //继续上锁

    // do something 3
    f << msg << val << std::endl;
    std::cout << msg << val << std::endl;
    // 结束时析构guard会临时解锁
    // 这句话可要可不要，不写，析构的时候也会自动执行
    // guard.ulock();
}

std::mutex my_mutex;
std::unique_lock<std::mutex> guard(my_mutex);
std::mutex *ptx = guard.release();
ptx->unlock();
```

- unique_lock 的第二个参数
  - `std::adopt_lock` ：表示互斥量应该是一个已经被当前线程锁住的mutex对象
  - `std::try_to_lock`：尝试用mutex的lock()去锁定互斥量，如果没有锁定成功也会立即返回，不会阻塞
  - `std::defer_lock`：在初始化的时候并不锁住互斥量。 互斥量应该是一个没有当前线程锁住的 mutex对象
- unique_lock的成员函数
  - `lock()`：加锁
  - `unlock()`：解锁，unique_lock析构时也会自动解锁
  - `try_lock()`：尝试给互斥量加锁，拿到锁返回true，拿不到锁返回false，函数不阻塞
  - `release()`：返回它所管理的mutex对象指针，并释放所有权，如果原来mutex对象处于加锁状态，需要接管过来并负责解锁

通过lock/unlock可以比较灵活的控制锁的范围，减小锁的粒度。

**`unique_lock`和`lock_guard`都不能复制，`lock_guard`不能移动，但是`unique_lock`可以**

```c++
// unique_lock 可以移动，不能复制
std::unique_lock<std::mutex> guard1(_mu);
std::unique_lock<std::mutex> guard2 = guard1;  // error
std::unique_lock<std::mutex> guard2 = std::move(guard1); // ok

// lock_guard 不能移动，不能复制
std::lock_guard<std::mutex> guard1(_mu);
std::lock_guard<std::mutex> guard2 = guard1;  // error
std::lock_guard<std::mutex> guard2 = std::move(guard1); // error
```

**std::condition_variable**

- `notify_one()`唤醒处于wait中的其中一个条件变量
- `notify_all()`可以同时唤醒所有处于`wait`状态的条件变量
- `wait()`函数在阻塞线程时，会先调用互斥锁的unlock()函数，然后再将当前线程阻塞，在被唤醒后（由其他线程通知），又会继续调用lock()来获取锁，保护后面的队列操作，然后函数返回。而lock_guard没有lock和unlock接口，而unique_lock提供了。这就是必须使用unique_lock的原因。

```cpp
#include <iostream>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

std::deque<int> q;
std::mutex mu;
std::condition_variable cond;

void function_1() {
    int count = 10;
    while (count > 0) {
        std::unique_lock<std::mutex> locker(mu);
        q.push_front(count);
        locker.unlock();
        cond.notify_one();  // Notify one waiting thread, if there is one.
        std::this_thread::sleep_for(std::chrono::seconds(1));
        count--;
    }
}

void function_2() {
    int data = 0;
    while ( data != 1) {
        std::unique_lock<std::mutex> locker(mu);
        while(q.empty())
            cond.wait(locker); // Unlock mu and wait to be notified
        data = q.back();
        q.pop_back();
        locker.unlock();
        std::cout << "t2 got a value from t1: " << data << std::endl;
    }
}
int main() {
    std::thread t1(function_1);
    std::thread t2(function_2);
    t1.join();
    t2.join();
    return 0;
}
```

还可以将`cond.wait(locker);`换一种写法，`wait()`的第二个参数可以传入一个函数表示检查条件，这里使用`lambda`函数最为简单，如果这个函数返回的是`true`，`wait()`函数不会阻塞会直接返回，如果这个函数返回的是`false`，`wait()`函数就会阻塞着等待唤醒，如果被伪唤醒，会继续判断函数返回值。

```c++
void function_2() {
    int data = 0;
    while ( data != 1) {
        std::unique_lock<std::mutex> locker(mu);
        cond.wait(locker, [](){ return !q.empty();} );  // Unlock mu and wait to be notified
        data = q.back();
        q.pop_back();
        locker.unlock();
        std::cout << "t2 got a value from t1: " << data << std::endl;
    }
}
```

`wait_for`

wait_for和wait的区别在于它会接受一个时间段，在当前线程收到通知，或者在指定的时间内，它都会阻塞，当超时，或者收到了通知达到条件，wait_for返回，剩下的和wait一致。

```c++
unconditional (1)
template <class Rep, class Period>
  cv_status wait_for (unique_lock<mutex>& lck,
                      const chrono::duration<Rep,Period>& rel_time);

predicate (2)
template <class Rep, class Period, class Predicate>
       bool wait_for (unique_lock<mutex>& lck,
                      const chrono::duration<Rep,Period>& rel_time, Predicate pred);
```

`wait_until`

wait_until和wait_for类似，只不过时间范围被替换成了时间点。

```c++
unconditional (1)
template <class Clock, class Duration>
  cv_status wait_until (unique_lock<mutex>& lck,
                        const chrono::time_point<Clock,Duration>& abs_time);

predicate (2)
template <class Clock, class Duration, class Predicate>
       bool wait_until (unique_lock<mutex>& lck,
                        const chrono::time_point<Clock,Duration>& abs_time,
                        Predicate pred);
```

https://segmentfault.com/a/1190000039195800

**std::condition_variable_any 介绍**

与 std::condition_variable 类似，只不过 std::condition_variable_any 的 wait 函数可以接受任何 lockable 参数，而 std::condition_variable 只能接受 `std::unique_lock<std::mutex>` 类型的参数，除此以外，和 std::condition_variable 几乎完全一样。

**std::notify_all_at_thread_exit**

notify_all_at_thread_exit是一个函数，原型如下：

```
void notify_all_at_thread_exit (condition_variable& cond, unique_lock<mutex> lck);
```

当调用该函数的线程结束时，所有在cond上等待的线程都会收到通知。

https://www.cnblogs.com/haippy/p/3252041.html

**实现自旋锁**

```c++
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>

std::atomic_flag lock = ATOMIC_FLAG_INIT;

void f(int n)
{
    for (int cnt = 0; cnt < 100; ++cnt) {
        while (lock.test_and_set(std::memory_order_acquire))  // acquire lock
             ; // spin
        std::cout << "Output from thread " << n << '\n';
        lock.clear(std::memory_order_release);               // release lock
    }
}

int main()
{
    std::vector<std::thread> v;
    for (int n = 0; n < 10; ++n) {
        v.emplace_back(f, n);
    }
    for (auto& t : v) {
        t.join();
    }
}
```

#### C++11内存模型

C++11 中的 atomic library 中定义了以下6种语义来对内存操作的行为进行约定，这些语义分别规定了不同的内存操作在其它线程中的可见性问题：

```c++
memory_order {
  memory_order_relaxed,
  memory_order_consume,
  memory_order_acquire,
  memory_order_release,
  memory_order_acq_rel,
  memory_order_seq_cst
};
```

**顺序一致性模型**(std::memory_order_seq_cst)

对所有以 memory_order_seq_cst 方式进行的内存操作，不管它们是不是分散在不同的 cpu 中同时进行，这些操作所产生的效果最终都要求有一个全局的顺序，而且这个顺序在各个相关的线程看起来是一致的。

**acquire-release 模型**(std::memory_order_consume, std::memory_order_acquire, std::memory_order_release, std::memory_order_acq_rel)

release语义用于写操作，acquire语义则用于读操作，它们结合起来表示这样一个约定：如果一个线程A对一块内存 m 以 release 的方式进行写操作，那么在线程 A 中，所有在该 release 操作之前进行的内存写操作，都在另一个线程 B 对内存 m 以 acquire 的方式进行读操作后，变得可见。

**relaxed 模型**(std::memory_order_relaxed)

不保证内存顺序，只保证原子性和当前原子变量写顺序的一致性。

https://blog.csdn.net/hyman_yx/article/details/52208947

https://www.huliujia.com/blog/f85f72a3b3e3018ffe9c9d3c15dda0f5db079859/

#### 迭代器：++it、it++哪个好，为什么

1) 前置返回一个引用，后置返回一个对象
2) 前置不会产生临时对象，后置产生临时对象，临时对象会导致效率降低

```c++
// ++i实现代码为：
int& operator++()
{
   *this += 1;
   return *this;
}

//i++实现代码为：                 
int operator++(int)                 
{
   int temp = *this;                   
   ++*this;                       
   return temp;                  
} 
```





























