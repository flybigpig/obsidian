在 C++ 中，`class` 和 `struct` 都用于定义数据结构，二者之间有许多相似之处，但也有一些关键的区别。下面是对它们的比较和分析：

### 1. **默认访问控制权限**

#### `struct`：

- 默认情况下，`struct` 中的成员是 **公有的 (public)**，这意味着它们可以在类的外部直接访问。

#### `class`：

- 默认情况下，`class` 中的成员是 **私有的 (private)**，这意味着它们只能在类的内部访问，外部代码无法直接访问。

### 示例：

```
#include <iostream>
using namespace std;

// struct 中的成员默认为 public
struct MyStruct {
    int x;  // 默认 public
    void display() {
        cout << "x = " << x << endl;
    }
};

// class 中的成员默认为 private
class MyClass {
    int x;  // 默认 private
public:
    void setX(int val) {
        x = val;
    }
    void display() {
        cout << "x = " << x << endl;
    }
};

int main() {
    MyStruct s;
    s.x = 10;  // 直接访问成员
    s.display();

    MyClass c;
    c.setX(20);
    c.display();  // 通过公共成员函数访问

    return 0;
}
```

### 输出：

```
x = 10
x = 20
```

### 2. **继承访问权限**

继承时，`class` 和 `struct` 的访问权限也有所不同。

- 在 `struct` 中，继承的默认访问控制是 **公有继承**，即基类的公有成员仍然是派生类的公有成员。
- 在 `class` 中，继承的默认访问控制是 **私有继承**，即基类的公有成员在派生类中变为私有成员。

### 示例：

```
#include <iostream>
using namespace std;

struct BaseStruct {
    int x;  // 默认 public
    BaseStruct() : x(10) {}
};

class BaseClass {
public:
    int x;  // public
    BaseClass() : x(20) {}
};

struct DerivedStruct : public BaseStruct {
    void display() {
        cout << "Base x = " << x << endl;  // 继承了 public 的成员
    }
};

class DerivedClass : public BaseClass {
public:
    void display() {
        cout << "Base x = " << x << endl;  // 继承了 public 的成员
    }
};

int main() {
    DerivedStruct ds;
    ds.display();  // 从 BaseStruct 继承了 public 成员

    DerivedClass dc;
    dc.display();  // 从 BaseClass 继承了 public 成员

    return 0;
}
```

### 输出：

```
Base x = 10
Base x = 20
```

### 3. **默认构造函数与析构函数**

- `struct` 和 `class` 都可以有构造函数和析构函数，并且行为一样。区别主要在于访问控制权限，`struct` 默认是公有的，而 `class` 默认是私有的。

### 4. **语义上的区别**

虽然 `class` 和 `struct` 在 C++ 中几乎没有区别，二者之间通常有语义上的不同。传统上：

- `struct` 用于表示简单的数据结构，通常包含数据成员，并且可以作为一种“简单的数据载体”。
- `class` 用于表示包含复杂逻辑、功能和行为的对象，强调数据封装和抽象。

### 5. **可用的功能**

`class` 和 `struct` 在功能上是等价的。两者都支持：

- 构造函数、析构函数
- 成员函数
- 成员变量
- 继承
- 多态（虚函数）

### 6. **C++ 代码风格习惯**

- 在 C++ 编程中，`struct` 更倾向于用于 **简洁的数据结构**，尤其是在需要与 C 代码兼容时使用。
- `class` 通常用于 **更复杂的对象建模**，其中包括数据封装、继承、多态等面向对象的特性。

### 示例总结：

```
#include <iostream>
using namespace std;

// struct 的使用
struct Person {
    string name;  // 默认 public
    int age;      // 默认 public
};

class Employee {
public:
    string name;  // public
    int age;      // public

    Employee(string n, int a) : name(n), age(a) {}

    void display() {
        cout << "Name: " << name << ", Age: " << age << endl;
    }
};

int main() {
    // 使用 struct 创建对象
    Person p = {"John", 25};
    cout << "Person: " << p.name << ", Age: " << p.age << endl;

    // 使用 class 创建对象
    Employee e("Alice", 30);
    e.display();

    return 0;
}
```

### 输出：

```
Person: John, Age: 25
Name: Alice, Age: 30
```

### 总结：

|   |   |   |
|---|---|---|
|特性|`struct`|`class`|
|默认访问权限|公有 (`public`)|私有 (`private`)|
|继承访问权限|公有继承 (`public`)|私有继承 (`private`)|
|用途|主要用于简单数据结构|用于复杂的面向对象建模|
|默认成员权限|公有 (`public`)|私有 (`private`)|
|功能|与 `class` 相同，支持构造函数、析构函数、继承等|与 `struct` 相同，支持构造函数、析构函数、继承等|

总结来说，`struct` 和 `class` 的主要区别在于 **默认的访问权限**，而在其他方面，它们几乎是等价的。在实际使用中，可以根据访问权限和语义上的需求选择使用 `struct` 或 `class`。