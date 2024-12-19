C++ 是一种广泛使用的高级编程语言，以下是一些 C++ 的基础知识：

**一、基本结构**

  

1.  **头文件（#include）**
    
      
    
    -   头文件包含了程序中要使用的函数和类的声明。例如，`#include <iostream>`用于输入输出操作。这个头文件提供了`cout`（标准输出流）和`cin`（标准输入流）等对象。
    -   还有`<string>`头文件用于处理字符串，`<vector>`头文件用于使用动态数组（向量）等。
    
      
    
2.  **主函数（main）**
    
      
    
    -   每个 C++ 程序都有一个主函数`int main()`，它是程序的入口点。程序从主函数开始执行，并且主函数返回一个整数值给操作系统。
    -   例如：
    
      
      
    
    ```
    int main() {
        // 代码内容
        return 0;
    }
    ```
    
      
    
    -   返回值 0 通常表示程序正常结束，非零值可以用于表示错误代码等情况。
    
      
    
3.  **命名空间（namespace）**
    
      
    
    -   命名空间用于避免名称冲突。例如，`std`是 C++ 标准库的命名空间。当使用`std`中的对象如`cout`时，需要写成`std::cout`。也可以使用`using namespace std;`语句来避免每次都写`std::`，不过这种做法在大型项目中可能会导致名称冲突，不推荐使用。
    
      
      
    
    ```
    #include <iostream>
    using namespace std;
    int main() {
        cout << "Hello, World!" << endl;
        return 0;
    }
    ```
    
      
    

**二、数据类型**

  

1.  **基本数据类型**
    
      
    
    -   **整数类型**：
        
        -   `int`：通常用于存储整数，它的大小一般是 4 个字节（在 32 位系统中），取值范围依赖于系统。例如`int num = 5;`。
        -   `long`、`long long`：用于存储更大范围的整数。`long`的大小至少是 4 个字节，`long long`通常是 8 个字节。
        
          
        
    -   **浮点类型**：
        
        -   `float`：用于存储单精度浮点数，占用 4 个字节。例如`float f = 3.14f;`（注意这里的`f`后缀表示是单精度浮点数）。
        -   `double`：用于存储双精度浮点数，占用 8 个字节，精度比`float`高。例如`double d = 3.1415926;`。
        
          
        
    -   **字符类型**：
        
        -   `char`：用于存储单个字符，占用 1 个字节。例如`char ch = 'A';`。字符在 C++ 中本质上是以 ASCII 码（或其他字符编码）的整数值存储的。
        
          
        
    -   **布尔类型**：
        
        -   `bool`：用于存储真（`true`）或假（`false`）的值。例如`bool flag = true;`。
        
          
        
    
      
    
2.  **复合数据类型**
    
      
    
    -   **数组**：
        
        -   数组是一组相同类型的数据元素的集合。例如，`int arr[5];`声明了一个包含 5 个整数的数组。可以通过索引访问数组元素，索引从 0 开始，如`arr[0] = 1;`。
        
          
        
    -   **结构体（struct）**：
        
        -   结构体允许用户将不同类型的数据组合在一起。例如：
        
          
          
        
        ```
        struct Student {
            int age;
            std::string name;
        };
        Student s;
        s.age = 20;
        s.name = "John";
        ```
        
          
        
    -   **类（class）**：
        
        -   类是 C++ 中面向对象编程的核心。它包含数据成员（变量）和成员函数（方法）。例如：
        
          
          
        
        ```
        class Circle {
        public:
            double radius;
            double area() {
                return 3.14159 * radius * radius;
            }
        };
        Circle c;
        c.radius = 2.0;
        double a = c.area();
        ```
        
          
        
    
      
    

**三、运算符**

  

1.  **算术运算符**
    
    -   `+`（加法）、`-`（减法）、`*`（乘法）、`/`（除法）、`%`（取余）。例如：
    
      
      
    
    ```
    int a = 10;
    int b = 3;
    int sum = a + b;
    int diff = a - b;
    int product = a * b;
    int quotient = a / b;
    int remainder = a % b;
    ```
    
      
    
2.  **关系运算符**
    
    -   `==`（等于）、`!=`（不等于）、`>`（大于）、`>=`（大于等于）、`<`（小于）、`<=`（小于等于）。它们用于比较两个值，返回布尔类型的值。例如：
    
      
      
    
    ```
    int x = 5;
    int y = 3;
    bool isEqual = x == y;
    bool isGreater = x > y;
    ```
    
      
    
3.  **逻辑运算符**
    
    -   `&&`（逻辑与）、`||`（逻辑或）、`!`（逻辑非）。例如：
    
      
      
    
    ```
    bool condition1 = true;
    bool condition2 = false;
    bool result1 = condition1 && condition2;
    bool result2 = condition1 || condition2;
    bool result3 =!condition2;
    ```
    
      
    

**四、控制结构**

  

1.  **条件语句（if - else）**
    
    -   根据条件执行不同的代码块。例如：
    
      
      
    
    ```
    int num = 10;
    if (num > 5) {
        std::cout << "The number is greater than 5." << endl;
    } else {
        std::cout << "The number is less than or equal to 5." << endl;
    }
    ```
    
      
    
2.  **循环语句**
    
    -   **for 循环**：
        
        -   用于已知循环次数的情况。例如：
        
          
          
        
        ```
        for (int i = 0; i < 10; i++) {
            std::cout << i << " ";
        }
        ```
        
          
        
    -   **while 循环**：
        
        -   只要条件为真就会一直执行循环体。例如：
        
          
          
        
        ```
        int j = 0;
        while (j < 5) {
            std::cout << j << " ";
            j++;
        }
        ```
        
          
        
    -   **do - while 循环**：
        
        -   先执行一次循环体，然后再判断条件是否为真。例如：
        
          
          
        
        ```
        int k = 0;
        do {
            std::cout << k << " ";
            k++;
        } while (k < 3);
        ```