
Java Stream API 是 Java 8 引入的一项强大特性，用于处理集合（例如 List、Set 和 Map）中的数据。Stream 使得处理数据的任务变得更加简洁和易读，尤其是进行过滤、映射、排序、聚合等操作时。

以下是一些关于 Java Stream 的基本概念和示例，包括如何使用 Stream API 执行常见操作。

### 1. 创建 Stream

你可以从集合、数组、文件等多种数据源创建流。

```java
import java.util.Arrays;
import java.util.List;
import java.util.stream.Stream;

public class CreateStreamExample {
    public static void main(String[] args) {
        // 从集合创建 Stream
        List<String> list = Arrays.asList("a", "b", "c");
        Stream<String> streamFromList = list.stream();

        // 从数组创建 Stream
        String[] array = {"d", "e", "f"};
        Stream<String> streamFromArray = Arrays.stream(array);

        // 直接创建 Stream
        Stream<String> streamDirect = Stream.of("g", "h", "i");
    }
}
```

### 2. 中间操作 (Intermediate Operations)

中间操作用于对 Stream 进行处理，这些操作是惰性求值的（即实际不会执行，直到遇到终端操作）。

#### 常见中间操作：

- **filter**：过滤元素
- **map**：转换每个元素
- **sorted**：排序元素

```java
import java.util.List;
import java.util.stream.Collectors;

public class IntermediateOperationsExample {
    public static void main(String[] args) {
        List<String> names = Arrays.asList("Alice", "Bob", "Charlie", "David", "Edward");

        // 过滤出长度大于 3 的名字，并转换为大写
        List<String> filteredAndMapped = names.stream()
                .filter(name -> name.length() > 3)
                .map(String::toUpperCase)
                .collect(Collectors.toList());

        System.out.println(filteredAndMapped); // 输出: [ALICE, CHARLIE, DAVID, EDWARD]
    }
}
```

### 3. 终端操作 (Terminal Operations)

终端操作用于生成最终结果，通常会消耗 Stream。

#### 常见终端操作：

- **forEach**：遍历元素
- **collect**：收集元素
- **reduce**：聚合元素

```java
import java.util.Arrays;
import java.util.List;

public class TerminalOperationsExample {
    public static void main(String[] args) {
        List<Integer> numbers = Arrays.asList(1, 2, 3, 4, 5);

        // 遍历并打印每个元素
        numbers.stream().forEach(num -> System.out.println(num));

        // 计算总和
        int sum = numbers.stream().reduce(0, Integer::sum);
        System.out.println("Sum: " + sum); // 输出: Sum: 15
    }
}
```

### 4. 组合操作

可以将中间和终端操作链式组合在一起，形成更复杂的流处理过程。

```java
import java.util.Arrays;
import java.util.List;

public class CombinedOperationsExample {
    public static void main(String[] args) {
        List<Integer> numbers = Arrays.asList(1, 2, 3, 4, 5, 6);

        // 计算偶数的平方和
        int evenSquaresSum = numbers.stream()
                .filter(num -> num % 2 == 0)
                .map(num -> num * num) // 或者使用 Math::pow(num, 2)
                .reduce(0, Integer::sum);

        System.out.println("Even Squares Sum: " + evenSquaresSum); // 输出: Even Squares Sum: 56
    }
}
```

### 总结

Java Stream API 提供了一种函数式编程的方式来处理集合，让数据处理的代码更加简洁和易于理解。你可以使用流来进行过滤、映射、排序和聚合等操作。它的惰性求值特性使得只有在需要结果时才会进行计算，从而提高了性能。