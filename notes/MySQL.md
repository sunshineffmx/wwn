#### 数据类型

**1. 数值类型**

包括整型、浮点型、定点型。

整型经常被用到，比如 tinyint、int、bigint 。默认是有符号的，若只需存储无符号值，可增加 unsigned 属性（int unsigned）。

<img src="https://segmentfault.com/img/bVcOoeq" alt="image.png" style="zoom: 80%;" />

浮点型主要有 float，double 两个，浮点型在数据库中存放的是近似值，例如float(6,3)，如果插入一个数123.45678，实际数据库里存的是123.457，但总个数还以实际为准，即6位，整数部分最大是3位。

<img src="https://segmentfault.com/img/bVcOoer" alt="image.png" style="zoom: 80%;" />

定点型字段类型有 DECIMAL 一个，主要用于存储有精度要求的小数。M是最大位数（精度），范围是1到65。可不指定，默认值是10。D是小数点右边的位数（小数位）。范围是0到30，并且不能大于M，可不指定，默认值是0。

<img src="https://segmentfault.com/img/bVcOoes" alt="image.png" style="zoom:80%;" />

**2. 字符串类型**

<img src="https://segmentfault.com/img/bVcOoex" alt="image.png" style="zoom:80%;" />

char 类型是定长的，MySQL 总是根据定义的字符串长度分配足够的空间。当保存 char 值时，在它们的右边填充空格以达到指定的长度，当检索到 char 值时，尾部的空格被删除掉。varchar 类型用于存储可变长字符串，存储时，如果字符没有达到定义的位数，也不会在后面补空格。

**3. 日期时间类型**

![image.png](https://segmentfault.com/img/bVcOoey)

#### 检索数据

```mysql
select prod_name from products;
select distinct vend_id from products;		# 只返回不同的vend_id行
select prod_name from products order by prod_name;	# 以字母顺序排序数据
select prod_name, prod_price from products where prod_pice = 2.50; # where子句指定过滤条件
select prod_name, prod_price from products where prod_pice between 5 and 10; # 范围值检查
select prod_id, prod_price, prod_name from products where vend_id = 1003 and prod_price <= 10;
select prod_id, prod_name from products where prod_name like 'jet%';	# 通配符%表示任何字符出现任意次数,通配符_只匹配单个任意字符

select avg(prod_price) as avg_price from products;	# 汇总数据，返回所有列的平均值
select count(*) as num_cust from customers;	# 对表中行的数目进行计数，不管表列中包含的是空值（NULL）还是非空值
select max(prod_price) as max_price from products;
select min(prod_price) as min_price from products;
select sum(quantity) as iterms_ordered from orderitems where order_num = 20005; # 计算指定列值的和

# 分组数据，以便能汇总表内容的子集
select vend_id, count(*) as num_prods from products group by vend_id;
select cust_id, count(*) as orders from orders group by cust_id having count(*) >= 2;	# having过滤分组

# 子查询
select cust_id from orders
where order_num in (select order_num from orderitems where prod_id = 'TNT2');
```

#### 连接表

外键：某个表中的一列，它包含另一个表的主键值，定义了两个表之间的关系。

使用连接来从多个表检索出数据。

```mysql
select vend_name, prod_name, prod_price
from vendors, products
where vendors.vend_id = products.vend_id
order by vend_name, prod_name;
```

自然连接：要求两个关系表中进行比较的必须是相同的属性列，无需添加连接条件，并且在结果中消除重复的属性列

```mysql
select * from studuent natural join class
```

内连接：使用比较运算符根据每个表共有的列的值匹配两个表中的行，如果在表A中找得到但是在B中没有（或者相反）的数据不予以显示。

自然连接要求是同名属性列的比较，而内连接则不要求两属性列同名，可以用using或on来指定某两列字段相同的连接条件。并且内连接会将名称相同的属性列全部保留，而不会去掉。

```mysql
SELECT * FROM student INNER JOIN class ON student.c_id = class.c_id;
```

外连接

左外连接：获取左表所有记录，即使右表没有对应匹配的记录。

右外连接：获取右表所有记录，即使左表没有对应匹配的记录。

全连接：返回左表和右表中的所有行。当某行在另一个表中没有匹配行时，则另一个表的选择列表列包含空值。

```mysql
SELECT * FROM student LEFT JOIN class ON student.c_id = class.c_id;
SELECT * FROM student RIGHT JOIN class ON student.c_id = class.c_id;
```

https://segmentfault.com/a/1190000038203698

交叉连接：返回左表中的所有行，左表中的每一行与右表中的所有行组合。交叉联接也称作笛卡尔积。假设A表中有n条记录，B表表中有m条记录，则它们联合查询得到的笛卡尔积为：`n*m`

```mysql
SELECT * FROM student, class;
```

#### SQL语法中内连接、自连接、外连接（左、右、全）、交叉连接的区别分别是什么？

内连接：只有两个元素表相匹配的才能在结果集中显示。

外连接：左外连接: 左边为驱动表，驱动表的数据全部显示，匹配表的不匹配的不会显示。

右外连接:右边为驱动表，驱动表的数据全部显示，匹配表的不匹配的不会显示。全外连接：连接的表中不匹配的数据全部会显示出来。

交叉连接：笛卡尔效应，显示的结果是链接表数的乘积。

#### 组合查询

将多个查询（多条select语句）的结果作为单个查询结果返回。

```mysql
select vend_id, prod_id, prod_price from products where prod_price <= 5
union
select vend_id, prod_id, prod_price from products where vend_id IN (1001,1002);
```

















