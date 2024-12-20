在 C++ 中，`class` 用于定义类。类是对象的蓝图，它描述了一组数据（成员变量）和操作这些数据的函数（成员函数）。通过 `class`，你可以创建自定义的数据类型，使代码更加模块化和结构化。

### C++ 类的基本结构

```
#include <iostream>
using namespace std;

class MyClass {
public:
    // 成员变量
    int x;
    int y;

    // 构造函数
    MyClass(int a, int b) {
        x = a;
        y = b;
    }

    // 成员函数
    void display() {
        cout << "x: " << x << ", y: " << y << endl;
    }
};

int main() {
    // 创建类的对象
    MyClass obj(10, 20);

    // 调用成员函数
    obj.display();

    return 0;
}
```

### 关键部分说明：

1. **类定义：** 使用 `class` 关键字来定义一个类。

- `class MyClass { ... };` 定义了一个名为 `MyClass` 的类。

2. **访问控制符：**

- `public`: 公有成员，可以在类外部访问。通常成员函数放在这里。
- `private`: 私有成员，只能在类的内部访问。通常成员变量放在这里。默认情况下，类的成员是私有的。
- `protected`: 保护成员，可以在类的内部及派生类中访问。

3. **成员变量：** 类的数据成员（通常是私有的），在类内部定义。

- `int x, y;` 是 `MyClass` 类的两个成员变量。

4. **构造函数：** 用来初始化对象。每次创建对象时会自动调用构造函数。

- `MyClass(int a, int b)` 是构造函数，接收两个参数来初始化 `x` 和 `y`。

5. **成员函数：** 类中的操作函数，操作对象的成员变量。

- `void display()` 是一个成员函数，用来显示对象的属性。

6. **对象创建：** 在 `main()` 函数中，使用 `MyClass obj(10, 20);` 创建了一个 `MyClass` 类型的对象 `obj`，并调用构造函数初始化 `x` 和 `y`。
7. **成员函数调用：** 使用 `obj.display();` 调用 `obj` 对象的 `display()` 成员函数。

### 输出：

```
x: 10, y: 20
```

### 类的构造与析构函数

#### 1. **构造函数：**

- 构造函数是类的一种特殊成员函数，它在对象创建时被自动调用，用来初始化对象的成员变量。
- 构造函数的名字与类名相同。
- 如果没有提供构造函数，编译器会生成一个默认构造函数。

#### 2. **析构函数：**

- 析构函数是一个特殊的成员函数，在对象生命周期结束时自动调用，通常用于清理资源（如动态内存释放）。
- 析构函数的名字是类名的前面加一个 `~` 符号。
- 每个类只能有一个析构函数。

```
class MyClass {
public:
    // 构造函数
    MyClass() {
        cout << "构造函数被调用" << endl;
    }

    // 析构函数
    ~MyClass() {
        cout << "析构函数被调用" << endl;
    }
};

int main() {
    MyClass obj;  // 创建对象时会调用构造函数
    // 对象生命周期结束时会调用析构函数
    return 0;
}
```

### 类的继承

C++ 支持面向对象编程中的继承机制，可以创建基类和派生类。派生类继承了基类的成员，并可以扩展或重写基类的功能。

```
#include <iostream>
using namespace std;

// 基类
class Animal {
public:
    void eat() {
        cout << "Animal is eating." << endl;
    }
};

// 派生类
class Dog : public Animal {
public:
    void bark() {
        cout << "Dog is barking." << endl;
    }
};

int main() {
    Dog d;
    d.eat();  // 从基类继承的方法
    d.bark(); // 派生类的方法
    return 0;
}
```

### 类的多态性

多态性是面向对象编程的一个重要特性，它允许不同的类以相同的接口调用各自不同的实现。通过虚函数（`virtual`）和函数重写（`override`），C++ 可以实现运行时的多态。

```
#include <iostream>
using namespace std;

class Animal {
public:
    virtual void sound() {
        cout << "Animal sound" << endl;
    }
};

class Dog : public Animal {
public:
    void sound() override {
        cout << "Bark!" << endl;
    }
};

class Cat : public Animal {
public:
    void sound() override {
        cout << "Meow!" << endl;
    }
};

int main() {
    Animal* a;
    Dog d;
    Cat c;

    a = &d;
    a->sound();  // 输出 "Bark!"

    a = &c;
    a->sound();  // 输出 "Meow!"

    return 0;
}
```

### 总结

C++ 中的 `class` 是面向对象编程的核心，它允许你创建自定义的数据类型、封装数据和操作、并支持继承与多态。理解类的构造函数、析构函数、继承、多态等特性是掌握 C++ 面向对象编程的基础。