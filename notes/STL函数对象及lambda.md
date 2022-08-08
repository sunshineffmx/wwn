## STL函数对象及lambda

### 函数对象(function object)

所谓function object，是一个定义了operator()的对象。

FunctionObjectType fo;

...

fo(...);

中表达式fo()掉function object fo的operator()，而非调用函数fo()。

#### 以function object为排序准则

```C++
#include <iostream>
#include <string>
#include <set>
#include <algorithm>
using namespace std;

class Person {
  public:
    string firstname() const;
    string lastname() const;
    ...
};

class PersonSortCriterion {
  public:
    bool operator() (const Person& p1, const Person& p2) const {
        return p1.lastname() < p2.lastname() ||
            (p1.lastname() == p2.lastname() && p1.firstname() < p2.firstname());
    }
};

int main()
{
    set<Person, PersonSortCriterion> coll;
    ...
    for (auto pos = coll.begin(); pos != coll.end(); ++pos) {
		...
    }
}
```

#### function object拥有内部状态

```C++
#include <iostream>
#include <list>
#include <algorithm>
#include <iterator>
using namespace std;

class IntSequence {
  private:
    int value;
  public:
    IntSequence(int initialValue)
      : value(initialValue) {
          
    }
    int operator() () {
        return ++value;
    }
};

int main() {
    list<int> coll;
    generate_n(back_inserter(coll), 9, IntSequence(1));
    generate(next(coll.begin()), prev(coll.end()), IntSequence(42));
}
```

### 运用Lambda

利用lambda表达式可以方便的定义和创建匿名函数。

lambda表达式完整的声明格式如下所示

```C++
[capture list](params list) mutable exception -> return type { function body}
```

各项具体含义如下：

- capture list：捕获外部变量列表
- params list：形参列表
- mutable指示符：用来说明是否可以修改捕获的变量
- exception：异常设定
- return type：返回类型
- function body：函数体

此外，我们还可以省略其中的某些成分来声明“不完整”的lambda表达式，常见的有以下几种：

```c++
[capture list](params list) -> return type {function body}
[capture list](params list) {function body}
[capture list]{function list}
```

```c++
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

bool cmp(int a, int b)
{
    return a < b;
}

int main()
{
    vector<int> myvec{3,2,5,7,3,2};
    vector<int> lbvec(myvec);
    sort(myvec.begin(), myvec.end(), cmp);
    sort(lbvec.begin(), lbvec.end(), [](int a, int b) -> bool {return a < b; });
}
```

外部变量的捕获方式有值捕获、引用捕获、隐式捕获

- 值捕获

```C++
int a = 123;
auto f = [a] { cout << a << endl; };
a = 321;
f();	// 输出123
```

- 引用捕获

```C++
int a = 123;
auto f = [&a] { cout << a << endl; };
a = 321;
f();	// 输出321
```

- 隐式捕获：[=]以值捕获的方式捕获外部变量，[&]以引用捕获的方式捕获外部变量。

```c++
int a = 123;
auto f = [=] { cout << a << endl; };
auto f1 = [&] { cout << a << endl; };
a = 321;
f();	// 输出123
f1();	// 输出321
```



```C++
#include <iostream>
int main() {
    auto plus10 = [] (int i) {
        return i + 10;
    };
    std::cout << "+10: " << plus10(7) << std::endl;
    
    auto plus10times2 = [] (int i) {
        return (i + 10) * 2;
    };
    std::cout << "+10 *2: " << plus10times2(7) << std::endl;
}
```

```C++
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> coll = {1,2,3,4,5,6,7,8};
    long sum = 0;
    for_each(coll.begin(), coll.end(), [&sum] (int elem) {
        sum += elem;
    });
    double mv = static_cast<double>(sum) / static_cast<double>(coll.size());
    cout << "mean value: " << mv << endl;
}
```

