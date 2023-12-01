# HashMap

`HahMap`其实就是`ArrayList`和`LinkedList`的数据结构加上`hashCode`和`equals`方法的思想设计出来的。没有理解上述说的知识点的同学可以翻开我过往的文章记录。

下面我就以面试问答的形式学习我们的——`HashMap`（源码分析基于JDK8，辅以JDK7），问答内容只是对`HashMap`的一个总结归纳，因为现时已经有大牛把`HashMap`通俗易懂的剖析了一遍，我学习`HashMap`也是主要通过这篇文章学习的，

强烈推荐：美团点评技术团队的[Java 8系列之重新认识HashMap](https://link.jianshu.com?t=https://tech.meituan.com/java-hashmap.html)



### 1.

问：`HashMap`有用过吗？您能给我说说他的主要用途吗？

答：

- 有用过，我在平常工作中经常会用到`HashMap`这种数据结构，`HashMap`是基于`Map`接口实现的一种键-值对`<key,value>`的存储结构，允许`null`值，同时非有序，非同步(即线程不安全)。`HashMap`的底层实现是数组 + 链表 + 红黑树（JDK1.8增加了红黑树部分）。它存储和查找数据时，是根据键`key`的`hashCode`的值计算出具体的存储位置。`HashMap`最多只允许一条记录的键`key`为`null`，`HashMap`增删改查等常规操作都有不错的执行效率，是`ArrayList`和`LinkedList`等数据结构的一种折中实现。

示例代码：



```csharp
        // 创建一个HashMap，如果没有指定初始大小，默认底层hash表数组的大小为16
        HashMap<String, String> hashMap = new HashMap<String, String>();
        // 往容器里面添加元素
        hashMap.put("小明", "好帅");
        hashMap.put("老王", "坑爹货");
        hashMap.put("老铁", "没毛病");
        hashMap.put("掘金", "好地方");
        hashMap.put("王五", "别搞事");
        // 获取key为小明的元素 好帅
        String element = hashMap.get("小明");
        // value : 好帅
        System.out.println(element);
        // 移除key为王五的元素
        String removeElement = hashMap.remove("王五");
        // value : 别搞事
        System.out.println(removeElement);
        // 修改key为小明的元素的值value 为 其实有点丑
        hashMap.replace("小明", "其实有点丑");
        // {老铁=没毛病, 小明=其实有点丑, 老王=坑爹货, 掘金=好地方}
        System.out.println(hashMap);
        // 通过put方法也可以达到修改对应元素的值的效果
        hashMap.put("小明", "其实还可以啦,开玩笑的");
        // {老铁=没毛病, 小明=其实还可以啦,开玩笑的, 老王=坑爹货, 掘金=好地方}
        System.out.println(hashMap);
        // 判断key为老王的元素是否存在(捉奸老王)
        boolean isExist = hashMap.containsKey("老王");
        // true , 老王竟然来搞事
        System.out.println(isExist);
        // 判断是否有 value = "坑爹货" 的人
        boolean isHasSomeOne = hashMap.containsValue("坑爹货");
        // true 老王是坑爹货
        System.out.println(isHasSomeOne);
        // 查看这个容器里面还有几个家伙 value : 4
        System.out.println(hashMap.size());
```

- `HashMap`的底层实现是数组 + 链表 + 红黑树（JDK1.8增加了红黑树部分），核心组成元素有：

1. `int size;`用于记录`HashMap`实际存储元素的个数；
2. `float loadFactor;`负载因子（默认是0.75，此属性后面详细解释）。
3. `int threshold;`下一次扩容时的阈值，达到阈值便会触发扩容机制`resize`（阈值 threshold = 容器容量 capacity * 负载因子 load factor）。也就是说，在容器定义好容量之后，负载因子越大，所能容纳的键值对元素个数就越多。
4. `Node<K,V>[] table;` 底层数组，充当哈希表的作用，用于存储对应hash位置的元素`Node<K,V>`，此数组长度总是2的N次幂。（具体原因后面详细解释）

示例代码：



```dart
public class HashMap<K,V> extends AbstractMap<K,V>
    implements Map<K,V>, Cloneable, Serializable {
·····

    /* ---------------- Fields -------------- */

    /**
     * 哈希表，在第一次使用到时进行初始化，重置大小是必要的操作，
     * 当分配容量时，长度总是2的N次幂。
     */
    transient Node<K,V>[] table;

    /**
     * 实际存储的key - value 键值对 个数
     */
    transient int size;


    /**
     * 下一次扩容时的阈值 
     * (阈值 threshold = 容器容量 capacity * 负载因子 load factor).
     * @serial
     */
    int threshold;

    /**
     * 哈希表的负载因子
     *
     * @serial
     */
    final float loadFactor;

·····
}
```

- 其中`Node<K,V>[] table;`哈希表存储的核心元素是`Node<K,V>`,`Node<K,V>`包含：

1. `final int hash;`元素的哈希值，决定元素存储在`Node<K,V>[] table;`哈希表中的位置。由`final`修饰可知，当`hash`的值确定后，就不能再修改。
2. `final K key;` 键，由`final`修饰可知，当`key`的值确定后，就不能再修改。
3. `V value;` 值
4. `Node<K,V> next;` 记录下一个元素结点(单链表结构，用于解决hash冲突)

示例代码：



```java
    /**
     * 定义HashMap存储元素结点的底层实现
     */
    static class Node<K,V> implements Map.Entry<K,V> {
        final int hash;//元素的哈希值 由final修饰可知，当hash的值确定后，就不能再修改
        final K key;// 键，由final修饰可知，当key的值确定后，就不能再修改
        V value; // 值
        Node<K,V> next; // 记录下一个元素结点(单链表结构，用于解决hash冲突)

        
        /**
         * Node结点构造方法
         */
        Node(int hash, K key, V value, Node<K,V> next) {
            this.hash = hash;//元素的哈希值
            this.key = key;// 键
            this.value = value; // 值
            this.next = next;// 记录下一个元素结点
        }

        public final K getKey()        { return key; }
        public final V getValue()      { return value; }
        public final String toString() { return key + "=" + value; }

        /**
         * 为Node重写hashCode方法，值为：key的hashCode 异或 value的hashCode 
         * 运算作用就是将2个hashCode的二进制中，同一位置相同的值为0，不同的为1。
         */
        public final int hashCode() {
            return Objects.hashCode(key) ^ Objects.hashCode(value);
        }

        /**
         * 修改某一元素的值
         */
        public final V setValue(V newValue) {
            V oldValue = value;
            value = newValue;
            return oldValue;
        }

        /**
         * 为Node重写equals方法
         */
        public final boolean equals(Object o) {
            if (o == this)
                return true;
            if (o instanceof Map.Entry) {
                Map.Entry<?,?> e = (Map.Entry<?,?>)o;
                if (Objects.equals(key, e.getKey()) &&
                    Objects.equals(value, e.getValue()))
                    return true;
            }
            return false;
        }
    }
```

![img](https:////upload-images.jianshu.io/upload_images/4752096-11477b54e04a51b2.png?imageMogr2/auto-orient/strip|imageView2/2/w/970/format/webp)

hashMap内存结构图 - 图片来自于《美团点评技术团队文章》

### 2.

问：您能说说`HashMap`常用操作的底层实现原理吗？如存储`put(K key, V value)`，查找`get(Object key)`，删除`remove(Object key)`，修改`replace(K key, V value)`等操作。

答：

- 调用`put(K key, V value)`操作添加`key-value`键值对时，进行了如下操作：

1. 判断哈希表`Node<K,V>[] table`是否为空或者`null`，是则执行`resize()`方法进行扩容。
2. 根据插入的键值`key`的`hash`值，通过`(n - 1) & hash`当前元素的`hash`值 & `hash`表长度 - 1（实际就是 `hash`值 % `hash`表长度） 计算出存储位置`table[i]`。如果存储位置没有元素存放，则将新增结点存储在此位置`table[i]`。
3. 如果存储位置已经有键值对元素存在，则判断该位置元素的`hash`值和`key`值是否和当前操作元素一致，一致则证明是修改`value`操作，覆盖`value`即可。
4. 当前存储位置即有元素，又不和当前操作元素一致，则证明此位置`table[i]`已经发生了hash冲突，则通过判断头结点是否是`treeNode`，如果是`treeNode`则证明此位置的结构是红黑树，已红黑树的方式新增结点。
5. 如果不是红黑树，则证明是单链表，将新增结点插入至链表的最后位置，随后判断当前链表长度是否 大于等于 8，是则将当前存储位置的链表转化为红黑树。遍历过程中如果发现`key`已经存在，则直接覆盖`value`。
6. 插入成功后，判断当前存储键值对的数量 大于 阈值`threshold` 是则扩容。

![img](https:////upload-images.jianshu.io/upload_images/4752096-2a7e5b42fd503a6a.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

hashMap put方法执行流程图- 图片来自于《美团点评技术团队文章》

示例代码：



```dart
    /**
     * 添加key-value键值对
     *
     * @param key 键
     * @param value 值
     * @return 如果原本存在此key，则返回旧的value值，如果是新增的key-     
     *         value，则返回nulll
     */
    public V put(K key, V value) {
        //实际调用putVal方法进行添加 key-value 键值对操作
        return putVal(hash(key), key, value, false, true);
    }

    /**
     * 根据key 键 的 hashCode 通过 “扰动函数” 生成对应的 hash值
     * 经过此操作后，使每一个key对应的hash值生成的更均匀，
     * 减少元素之间的碰撞几率（后面详细说明）
     */
    static final int hash(Object key) {
        int h;
        return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
    }


    /**
     * 添加key-value键值对的实际调用方法（重点）
     *
     * @param hash key 键的hash值
     * @param key 键
     * @param value 值
     * @param onlyIfAbsent 此值如果是true, 则如果此key已存在value，则不执
     * 行修改操作 
     * @param evict 此值如果是false，哈希表是在初始化模式
     * @return 返回原本的旧值, 如果是新增，则返回null
     */
    final V putVal(int hash, K key, V value, boolean onlyIfAbsent,
                   boolean evict) {
        // 用于记录当前的hash表
        Node<K,V>[] tab; 
        // 用于记录当前的链表结点
        Node<K,V> p; 
        // n用于记录hash表的长度，i用于记录当前操作索引index
        int n, i;
        // 当前hash表为空
        if ((tab = table) == null || (n = tab.length) == 0)
            // 初始化hash表，并把初始化后的hash表长度值赋值给n
            n = (tab = resize()).length;
        // 1）通过 (n - 1) & hash 当前元素的hash值 & hash表长度 - 1
        // 2）确定当前元素的存储位置，此运算等价于 当前元素的hash值 % hash表的长度
        // 3）计算出的存储位置没有元素存在
        if ((p = tab[i = (n - 1) & hash]) == null)
            // 4) 则新建一个Node结点，在该位置存储此元素
            tab[i] = newNode(hash, key, value, null);
        else { // 当前存储位置已经有元素存在了(不考虑是修改的情况的话，就代表发生hash冲突了)
            // 用于存放新增结点
            Node<K,V> e; 
            // 用于临时存在某个key值
            K k;
            // 1)如果当前位置已存在元素的hash值和新增元素的hash值相等
            // 2)并且key也相等，则证明是同一个key元素，想执行修改value操作
            if (p.hash == hash &&
                ((k = p.key) == key || (key != null && key.equals(k))))
                e = p;// 将当前结点引用赋值给e
            else if (p instanceof TreeNode) // 如果当前结点是树结点
                // 则证明当前位置的链表已变成红黑树结构，则已红黑树结点结构新增元素
                e = ((TreeNode<K,V>)p).putTreeVal(this, tab, hash, key, value);
            else {// 排除上述情况，则证明已发生hash冲突，并hash冲突位置现时的结构是单链表结构
                for (int binCount = 0; ; ++binCount) {
                    //遍历单链表，将新元素结点放置此链表的最后一位
                    if ((e = p.next) == null) {
                        // 将新元素结点放在此链表的最后一位
                        p.next = newNode(hash, key, value, null);
                        // 新增结点后，当前结点数量是否大于等于 阈值 8 
                        if (binCount >= TREEIFY_THRESHOLD - 1) // -1 for 1st
                            // 大于等于8则将链表转换成红黑树
                            treeifyBin(tab, hash);
                        break;
                    }
                    // 如果链表中已经存在对应的key，则覆盖value
                    if (e.hash == hash &&
                        ((k = e.key) == key || (key != null && key.equals(k))))
                        break;
                    p = e;
                }
            }
            if (e != null) { // 已存在对应key
                V oldValue = e.value;
                if (!onlyIfAbsent || oldValue == null) //如果允许修改，则修改value为新值
                    e.value = value;
                afterNodeAccess(e);
                return oldValue;
            }
        }
        ++modCount;
        // 当前存储键值对的数量 大于 阈值 是则扩容
        if (++size > threshold)
           // 重置hash大小，将旧hash表的数据逐一复制到新的hash表中（后面详细讲解）
            resize();
        afterNodeInsertion(evict);
        // 返回null，则证明是新增操作，而不是修改操作
        return null;
    }
```

- 调用`get(Object key)`操作根据键`key`查找对应的`key-value`键值对时，进行了如下操作：

1. 先调用 `hash(key)`方法计算出 `key` 的 `hash`值
2. 根据查找的键值`key`的`hash`值，通过`(n - 1) & hash`当前元素的`hash`值  & `hash`表长度 - 1（实际就是 `hash`值 % `hash`表长度） 计算出存储位置`table[i]`，判断存储位置是否有元素存在 。

- 如果存储位置有元素存放，则首先比较头结点元素，如果头结点的`key`的`hash`值 和 要获取的`key`的`hash`值相等，并且 头结点的`key`本身 和要获取的 `key` 相等，则返回该位置的头结点。
- 如果存储位置没有元素存放，则返回`null`。

1. 如果存储位置有元素存放，但是头结点元素不是要查找的元素，则需要遍历该位置进行查找。
2. 先判断头结点是否是`treeNode`，如果是`treeNode`则证明此位置的结构是红黑树，以红色树的方式遍历查找该结点，没有则返回`null`。
3. 如果不是红黑树，则证明是单链表。遍历单链表，逐一比较链表结点，链表结点的`key`的`hash`值 和 要获取的`key`的`hash`值相等，并且 链表结点的`key`本身 和要获取的 `key` 相等，则返回该结点，遍历结束仍未找到对应`key`的结点，则返回`null`。

示例代码：



```dart
    /**
     * 返回指定 key 所映射的 value 值
     * 或者 返回 null 如果容器里不存在对应的key
     *
     * 更确切地讲，如果此映射包含一个满足 (key==null ? k==null :key.equals(k))
     * 的从 k 键到 v 值的映射关系，
     * 则此方法返回 v；否则返回 null。（最多只能有一个这样的映射关系。）
     *
     * 返回 null 值并不一定 表明该映射不包含该键的映射关系；
     * 也可能该映射将该键显示地映射为 null。可使用containsKey操作来区分这两种情况。 
     *
     * @see #put(Object, Object)
     */
    public V get(Object key) {
        Node<K,V> e;
        // 1.先调用 hash(key)方法计算出 key 的 hash值
        // 2.随后调用getNode方法获取对应key所映射的value值
        return (e = getNode(hash(key), key)) == null ? null : e.value;
    }


    /**
     * 获取哈希表结点的方法实现
     *
     * @param hash key 键的hash值
     * @param key 键
     * @return 返回对应的结点，如果结点不存在，则返回null
     */
    final Node<K,V> getNode(int hash, Object key) {
        // 用于记录当前的hash表 
        Node<K,V>[] tab; 
        // first用于记录对应hash位置的第一个结点，e充当工作结点的作用
        Node<K,V> first, e; 
        // n用于记录hash表的长度
        int n; 
        // 用于临时存放Key
        K k;
        // 通过 (n - 1) & hash 当前元素的hash值 & hash表长度 - 1
        // 判断当前元素的存储位置是否有元素存在 
        if ((tab = table) != null && (n = tab.length) > 0 &&
            (first = tab[(n - 1) & hash]) != null) {//元素存在的情况
           // 如果头结点的key的hash值 和 要获取的key的hash值相等
           // 并且 头结点的key本身 和要获取的 key 相等
            if (first.hash == hash && // always check first node 总是检查头结点
                ((k = first.key) == key || (key != null && key.equals(k))))
                // 返回该位置的头结点
                return first;
            if ((e = first.next) != null) {// 头结点不相等
                if (first instanceof TreeNode) // 如果当前结点是树结点
                    // 则证明当前位置的链表已变成红黑树结构
                    // 通过红黑树结点的方式获取对应key结点
                    return ((TreeNode<K,V>)first).getTreeNode(hash, key);
                do {// 当前位置不是红黑树，则证明是单链表
                    // 遍历单链表，逐一比较链表结点
                    // 链表结点的key的hash值 和 要获取的key的hash值相等
                    // 并且 链表结点的key本身 和要获取的 key 相等
                    if (e.hash == hash &&
                        ((k = e.key) == key || (key != null && key.equals(k))))
                        // 找到对应的结点则返回
                        return e;
                } while ((e = e.next) != null);
            }
        }
        // 通过上述查找均无找到，则返回null
        return null;
    }
```

- 调用`remove(Object key)`操作根据键`key`删除对应的`key-value`键值对时，进行了如下操作：

1. 先调用 `hash(key)`方法计算出 `key` 的 `hash`值
2. 根据查找的键值`key`的`hash`值，通过`(n - 1) & hash`当前元素的`hash`值  & `hash`表长度 - 1（实际就是 `hash`值 % `hash`表长度） 计算出存储位置`table[i]`，判断存储位置是否有元素存在 。

- 如果存储位置有元素存放，则首先比较头结点元素，如果头结点的`key`的`hash`值 和 要获取的`key`的`hash`值相等，并且 头结点的`key`本身 和要获取的 `key` 相等，则该位置的头结点即为要删除的结点，记录此结点至变量`node`中。
- 如果存储位置没有元素存放，则没有找到对应要删除的结点，则返回`null`。

1. 如果存储位置有元素存放，但是头结点元素不是要删除的元素，则需要遍历该位置进行查找。
2. 先判断头结点是否是`treeNode`，如果是`treeNode`则证明此位置的结构是红黑树，以红色树的方式遍历查找并删除该结点，没有则返回`null`。
3. 如果不是红黑树，则证明是单链表。遍历单链表，逐一比较链表结点，链表结点的`key`的`hash`值 和 要获取的`key`的`hash`值相等，并且 链表结点的`key`本身 和要获取的 `key` 相等，则此为要删除的结点，记录此结点至变量`node`中，遍历结束仍未找到对应`key`的结点，则返回`null`。
4. 如果找到要删除的结点`node`，则判断是否需要比较`value`也是否一致，如果`value`值一致或者不需要比较`value`值，则执行删除结点操作，删除操作根据不同的情况与结构进行不同的处理。

- 如果当前结点是树结点，则证明当前位置的链表已变成红黑树结构，通过红黑树结点的方式删除对应结点。
- 如果不是红黑树，则证明是单链表。如果要删除的是头结点，则当前存储位置`table[i]`的头结点指向删除结点的下一个结点。
- 如果要删除的结点不是头结点，则将要删除的结点的后继结点`node.next`赋值给要删除结点的前驱结点的`next`域，即`p.next = node.next;`。

1. `HashMap`当前存储键值对的数量 - 1，并返回删除结点。

示例代码：



```java
    /**
     * 从此映射中移除指定键的映射关系（如果存在）。
     *
     * @param  key 其映射关系要从映射中移除的键
     * @return 与 key 关联的旧值；如果 key 没有任何映射关系，则返回 null。
     *        （返回 null 还可能表示该映射之前将 null 与 key 关联。）
     */
    public V remove(Object key) {
        Node<K,V> e;
        // 1.先调用 hash(key)方法计算出 key 的 hash值
        // 2.随后调用removeNode方法删除对应key所映射的结点
        return (e = removeNode(hash(key), key, null, false, true)) == null ?
            null : e.value;
    }


    /**
     * 删除哈希表结点的方法实现
     *
     * @param hash 键的hash值
     * @param key 键
     * @param value 用于比较的value值，当matchValue 是 true时才有效, 否则忽略
     * @param matchValue 如果是 true 只有当value相等时才会移除
     * @param movable 如果是 false当执行移除操作时，不删除其他结点
     * @return 返回删除结点node，不存在则返回null
     */
    final Node<K,V> removeNode(int hash, Object key, Object value,
                               boolean matchValue, boolean movable) {
        // 用于记录当前的hash表
        Node<K,V>[] tab; 
        // 用于记录当前的链表结点
        Node<K,V> p; 
        // n用于记录hash表的长度，index用于记录当前操作索引index
        int n, index;
        // 通过 (n - 1) & hash 当前元素的hash值 & hash表长度 - 1
        // 判断当前元素的存储位置是否有元素存在 
        if ((tab = table) != null && (n = tab.length) > 0 &&
            (p = tab[index = (n - 1) & hash]) != null) {// 元素存在的情况
            // node 用于记录找到的结点，e为工作结点
            Node<K,V> node = null, e; 
            K k; V v;
           // 如果头结点的key的hash值 和 要获取的key的hash值相等
           // 并且 头结点的key本身 和要获取的 key 相等
           // 则证明此头结点就是要删除的结点
            if (p.hash == hash &&
                ((k = p.key) == key || (key != null && key.equals(k))))
                // 记录要删除的结点的引用地址至node中
                node = p;
            else if ((e = p.next) != null) {// 头结点不相等
                if (p instanceof TreeNode)// 如果当前结点是树结点
                    // 则证明当前位置的链表已变成红黑树结构
                    // 通过红黑树结点的方式获取对应key结点
                    // 记录要删除的结点的引用地址至node中
                    node = ((TreeNode<K,V>)p).getTreeNode(hash, key);
                else {// 当前位置不是红黑树，则证明是单链表
                    do {
                        // 遍历单链表，逐一比较链表结点
                        // 链表结点的key的hash值 和 要获取的key的hash值相等
                        // 并且 链表结点的key本身 和要获取的 key 相等
                        if (e.hash == hash &&
                            ((k = e.key) == key ||
                             (key != null && key.equals(k)))) {
                            // 找到则记录要删除的结点的引用地址至node中，中断遍历
                            node = e;
                            break;
                        }
                        p = e;
                    } while ((e = e.next) != null);
                }
            }
            // 如果找到要删除的结点，则判断是否需要比较value也是否一致
            if (node != null && (!matchValue || (v = node.value) == value ||
                                 (value != null && value.equals(v)))) {
                // value值一致或者不需要比较value值，则执行删除结点操作
                if (node instanceof TreeNode) // 如果当前结点是树结点
                    // 则证明当前位置的链表已变成红黑树结构
                    // 通过红黑树结点的方式删除对应结点
                    ((TreeNode<K,V>)node).removeTreeNode(this, tab, movable);
                else if (node == p) // node 和 p相等，则证明删除的是头结点
                    // 当前存储位置的头结点指向删除结点的下一个结点
                    tab[index] = node.next;
                else // 删除的不是头结点
                    // p是删除结点node的前驱结点，p的next改为记录要删除结点node的后继结点
                    p.next = node.next;
                ++modCount;
               // 当前存储键值对的数量 - 1
                --size;
                afterNodeRemoval(node);
                // 返回删除结点
                return node;
            }
        }
        // 不存在要删除的结点，则返回null
        return null;
    }
```

- 调用`replace(K key, V value)`操作根据键`key`查找对应的`key-value`键值对，随后替换对应的值`value`，进行了如下操作：

1. 先调用 `hash(key)`方法计算出 `key` 的 `hash`值
2. 随后调用`getNode`方法获取对应`key`所映射的`value`值 。
3. 记录元素旧值，将新值赋值给元素，返回元素旧值，如果没有找到元素，则返回`null`。

示例代码：



```csharp
    /**
     * 替换指定 key 所映射的 value 值
     *
     * @param key 对应要替换value值元素的key键
     * @param value 要替换对应元素的新value值
     * @return 返回原本的旧值，如果没有找到key对应的元素，则返回null
     * @since 1.8 JDK1.8新增方法
     */
    public V replace(K key, V value) {
        Node<K,V> e;
        // 1.先调用 hash(key)方法计算出 key 的 hash值
        // 2.随后调用getNode方法获取对应key所映射的value值
        if ((e = getNode(hash(key), key)) != null) {// 如果找到对应的元素
            // 元素旧值
            V oldValue = e.value;
            // 将新值赋值给元素
            e.value = value;
            afterNodeAccess(e);
            // 返回元素旧值
            return oldValue;
        }
        // 没有找到元素，则返回null
        return null;
    }
```

### 3.

问 1：您上面说，存放一个元素时，先计算它的hash值确定它的存储位置，然后再把这个元素放到对应的位置上，那万一这个位置上面已经有元素存在呢，新增的这个元素怎么办？

问 2：`hash`冲突（或者叫`hash`碰撞）是什么？为什么会出现这种现象，如何解决`hash`冲突？

答：

- `hash`冲突： 当我们调用`put(K key, V value)`操作添加`key-value`键值对，这个`key-value`键值对存放在的位置是通过扰动函数`(key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16)`计算键`key`的`hash`值。随后将 这个`hash`值 % 模上 哈希表`Node<K,V>[] table`的长度 得到具体的存放位置。所以`put(K key, V value)`多个元素，是有可能计算出相同的存放位置。此现象就是`hash`冲突或者叫`hash`碰撞。
- 例子如下：
   元素 A 的`hash`值 为 9，元素 B 的`hash`值 为 17。哈希表`Node<K,V>[] table`的长度为8。则元素 A 的存放位置为`9 % 8 = 1`，元素 B 的存放位置为`17 % 8 = 1`。两个元素的存放位置均为`table[1]`，发生了`hash`冲突。
- `hash`冲突的避免：既然会发生`hash`冲突，我们就应该想办法避免此现象的发生，解决这个问题最关键就是如果生成元素的`hash`值。Java是使用“扰动函数”生成元素的`hash`值。

示例代码：



```dart
   /**
    * JDK 7 的 hash方法
    */
    final int hash(int h) {

        h ^= k.hashCode();

        h ^= (h >>> 20) ^ (h >>> 12);
        return h ^ (h >>> 7) ^ (h >>> 4);
    }

   /**
    * JDK 8 的 hash方法
    */
    static final int hash(Object key) {
        int h;
        return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
    }
```

Java7做了4次16位右位移异或混合，Java 8中这步已经简化了，只做一次16位右位移异或混合，而不是四次，但原理是不变的。例子如下：

![img](https:////upload-images.jianshu.io/upload_images/4752096-08ad5d8cabd209f1.png?imageMogr2/auto-orient/strip|imageView2/2/w/586/format/webp)

扰动函数执行例子 - 图片来自于《知乎》 

右位移16位，正好是32bit的一半，自己的高半区和低半区做异或，就是为了混合原始哈希码的高位和低位，以此来加大低位的随机性。而且混合后的低位掺杂了高位的部分特征，这样高位的信息也被变相保留下来。

上述扰动函数的解释参考自：[JDK 源码中 HashMap 的 hash 方法原理是什么？](https://link.jianshu.com?t=https://www.zhihu.com/question/20733617)

- `hash`冲突解决：解决`hash`冲突的方法有很多，常见的有：开发定址法，
   再散列法，链地址法，公共溢出区法（详细说明请查看我的文章[JAVA基础-自问自答学hashCode和equals](https://link.jianshu.com?t=https://juejin.im/post/59b25f825188257e7e11500c)）。`HashMap`是使用链地址法解决`hash`冲突的，当有冲突元素放进来时，会将此元素插入至此位置链表的最后一位，形成单链表。但是由于是单链表的缘故，每当通过`hash % length`找到该位置的元素时，均需要从头遍历链表，通过逐一比较`hash`值，找到对应元素。如果此位置元素过多，造成链表过长，遍历时间会大大增加，最坏情况下的时间复杂度为`O(N)`，造成查找效率过低。所以当存在位置的链表长度 大于等于 8 时，`HashMap`会将链表 转变为 红黑树，红黑树最坏情况下的时间复杂度为`O(logn)`。以此提高查找效率。

### 4.

问：`HashMap`的容量为什么一定要是2的n次方？

答：

- 因为调用`put(K key, V value)`操作添加`key-value`键值对时，具体确定此元素的位置是通过  `hash`值 % 模上 哈希表`Node<K,V>[] table`的长度 `hash % length` 计算的。但是"模"运算的消耗相对较大，通过位运算`h & (length-1)`也可以得到取模后的存放位置，而位运算的运行效率高，但只有`length`的长度是2的n次方时，`h & (length-1)` 才等价于 `h % length`。
- 而且当数组长度为2的n次幂的时候，不同的key算出的index相同的几率较小，那么数据在数组上分布就比较均匀，也就是说碰撞的几率小，相对的，查询的时候就不用遍历某个位置上的链表，这样查询效率也就较高了。

例子：

![img](https:////upload-images.jianshu.io/upload_images/4752096-e4ea0052e83dc3e2.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/877/format/webp)

hash &  (length-1)运算过程.jpg

- 上图中，左边两组的数组长度是16（2的4次方），右边两组的数组长度是15。两组的`hash`值均为8和9。
- 当数组长度是15时，当它们和`1110`进行`&`与运算（相同为1，不同为0）时，计算的结果都是`1000`，所以他们都会存放在相同的位置`table[8]`中，这样就发生了`hash`冲突，那么查询时就要遍历链表，逐一比较`hash`值，降低了查询的效率。
- 同时，我们可以发现，当数组长度为15的时候，`hash`值均会与`14（1110）`进行`&`与运算，那么最后一位永远是0，而`0001`，`0011`，`0101`，`1001`，`1011`，`0111`，`1101`这几个位置永远都不能存放元素了，空间浪费相当大，更糟的是这种情况中，数组可以使用的位置比数组长度小了很多，这意味着进一步增加了碰撞的几率，减慢了查询的效率。

- 所以，`HashMap`的容量是2的n次方，有利于提高计算元素存放位置时的效率，也降低了`hash`冲突的几率。因此，我们使用`HashMap`存储大量数据的时候，最好先预先指定容器的大小为2的n次方，即使我们不指定为2的n次方，`HashMap`也会把容器的大小设置成最接近设置数的2的n次方，如，设置`HashMap`的大小为 7 ，则`HashMap`会将容器大小设置成最接近7的一个2的n次方数，此值为 8 。

上述回答参考自：[深入理解HashMap](https://link.jianshu.com?t=http://annegu.iteye.com/blog/539465)

示例代码：



```dart
    /**
     * 返回一个比指定数cap大的，并且大小是2的n次方的数
     * Returns a power of two size for the given target capacity.
     */
    static final int tableSizeFor(int cap) {
        int n = cap - 1;
        n |= n >>> 1;
        n |= n >>> 2;
        n |= n >>> 4;
        n |= n >>> 8;
        n |= n >>> 16;
        return (n < 0) ? 1 : (n >= MAXIMUM_CAPACITY) ? MAXIMUM_CAPACITY : n + 1;
    }
```

### 5.

问：`HashMap`的负载因子是什么，有什么作用？

答：负载因子表示哈希表空间的使用程度（或者说是哈希表空间的利用率）。

- 例子如下：
   底层哈希表`Node<K,V>[] table`的容量大小`capacity`为 16，负载因子`load factor`为 0.75，则当存储的元素个数`size = capacity 16 * load factor 0.75`等于 12 时，则会触发`HashMap`的扩容机制，调用`resize()`方法进行扩容。
- 当负载因子越大，则`HashMap`的装载程度就越高。也就是能容纳更多的元素，元素多了，发生`hash`碰撞的几率就会加大，从而链表就会拉长，此时的查询效率就会降低。
- 当负载因子越小，则链表中的数据量就越稀疏，此时会对空间造成浪费，但是此时查询效率高。
- 我们可以在创建`HashMap` 时根据实际需要适当地调整`load factor` 的值；如果程序比较关心空间开销、内存比较紧张，可以适当地增加负载因子；如果程序比较关心时间开销，内存比较宽裕则可以适当的减少负载因子。通常情况下，默认负载因子 (0.75) 在时间和空间成本上寻求一种折衷，程序员无需改变负载因子的值。
- 因此，如果我们在初始化`HashMap`时，就预估知道需要装载`key-value`键值对的容量`size`，我们可以通过`size / load factor` 计算出我们需要初始化的容量大小`initialCapacity`，这样就可以避免`HashMap`因为存放的元素达到阈值`threshold`而频繁调用`resize()`方法进行扩容。从而保证了较好的性能。

### 6.

问：您能说说`HashMap`和`HashTable`的区别吗？

答：`HashMap`和`HashTable`有如下区别：

1）容器整体结构：

- `HashMap`的`key`和`value`都允许为`null`，`HashMap`遇到`key`为`null`的时候，调用`putForNullKey`方法进行处理，而对`value`没有处理。
- `Hashtable`的`key`和`value`都不允许为`null`。`Hashtable`遇到`null`，直接返回`NullPointerException`。

2） 容量设定与扩容机制：

- `HashMap`默认初始化容量为 16，并且容器容量一定是2的n次方，扩容时，是以原容量 2倍 的方式 进行扩容。
- `Hashtable`默认初始化容量为 11，扩容时，是以原容量 2倍 再加 1的方式进行扩容。即`int newCapacity = (oldCapacity << 1) + 1;`。

3） 散列分布方式（计算存储位置）：

- `HashMap`是先将`key`键的`hashCode`经过扰动函数扰动后得到`hash`值，然后再利用 `hash & (length - 1)`的方式代替取模，得到元素的存储位置。
- `Hashtable`则是除留余数法进行计算存储位置的（因为其默认容量也不是2的n次方。所以也无法用位运算替代模运算），`int index = (hash & 0x7FFFFFFF) % tab.length;`。
- 由于`HashMap`的容器容量一定是2的n次方，所以能使用`hash & (length - 1)`的方式代替取模的方式计算元素的位置提高运算效率，但`Hashtable`的容器容量不一定是2的n次方，所以不能使用此运算方式代替。

4）线程安全（最重要）：

- `HashMap` 不是线程安全，如果想线程安全，可以通过调用`synchronizedMap(Map<K,V> m)`使其线程安全。但是使用时的运行效率会下降，所以建议使用`ConcurrentHashMap`容器以此达到线程安全。
- `Hashtable`则是线程安全的，每个操作方法前都有`synchronized`修饰使其同步，但运行效率也不高，所以还是建议使用`ConcurrentHashMap`容器以此达到线程安全。

因此，`Hashtable`是一个遗留容器，如果我们不需要线程同步，则建议使用`HashMap`，如果需要线程同步，则建议使用`ConcurrentHashMap`。

此处不再对Hashtable的源码进行逐一分析了，如果想深入了解的同学，可以参考此文章
 [Hashtable源码剖析](https://link.jianshu.com?t=http://blog.csdn.net/chdjj/article/details/38581035)

### 7.

问：您说`HashMap`不是线程安全的，那如果多线程下，它是如何处理的？并且什么情况下会发生线程不安全的情况？

答：

- `HashMap`不是线程安全的，如果多个线程同时对同一个`HashMap`更改数据的话，会导致数据不一致或者数据污染。如果出现线程不安全的操作时，`HashMap`会尽可能的抛出`ConcurrentModificationException`防止数据异常，当我们在对一个`HashMap`进行遍历时，在遍历期间，我们是不能对`HashMap`进行添加，删除等更改数据的操作的，否则也会抛出`ConcurrentModificationException`异常，此为fail-fast（快速失败）机制。从源码上分析，我们在`put,remove`等更改`HashMap`数据时，都会导致modCount的改变，当`expectedModCount != modCount`时，则抛出`ConcurrentModificationException`。如果想要线程安全，可以考虑使用`ConcurrentHashMap`。
- 而且，在多线程下操作`HashMap`，由于存在扩容机制，当`HashMap`调用`resize()`进行自动扩容时，可能会导致死循环的发生。

由于时间关系，我暂不带着大家一起去分析`resize()`方法导致死循环发生的现象造成原因了，迟点有空我会再补充上去，请见谅，大家可以参考如下文章：

[Java 8系列之重新认识HashMap](https://link.jianshu.com?t=https://tech.meituan.com/java-hashmap.html)

[谈谈HashMap线程不安全的体现](https://link.jianshu.com?t=http://www.importnew.com/22011.html)

### 8.

问：我们在使用`HashMap`时，选取什么对象作为`key`键比较好，为什么？

答：

- 可变对象：指创建后自身状态能改变的对象。换句话说，可变对象是该对象在创建后它的哈希值可能被改变。
- 我们在使用`HashMap`时，最好选择不可变对象作为`key`。例如`String`，`Integer`等不可变类型作为`key`是非常明智的。
- 如果`key`对象是可变的，那么`key`的哈希值就可能改变。在`HashMap`中可变对象作为Key会造成数据丢失。因为我们再进行`hash & (length - 1)`取模运算计算位置查找对应元素时，位置可能已经发生改变，导致数据丢失。

详细例子说明请参考：[危险！在HashMap中将可变对象用作Key](https://link.jianshu.com?t=http://www.importnew.com/13384.html)

# 总结

1. `HashMap`是基于`Map`接口实现的一种键-值对`<key,value>`的存储结构，允许`null`值，同时非有序，非同步(即线程不安全)。`HashMap`的底层实现是数组 + 链表 + 红黑树（JDK1.8增加了红黑树部分）。
2. `HashMap`定位元素位置是通过键`key`经过扰动函数扰动后得到`hash`值，然后再通过`hash & (length - 1)`代替取模的方式进行元素定位的。
3. `HashMap`是使用链地址法解决`hash`冲突的，当有冲突元素放进来时，会将此元素插入至此位置链表的最后一位，形成单链表。当存在位置的链表长度 大于等于 8 时，`HashMap`会将链表 转变为 红黑树，以此提高查找效率。
4. `HashMap`的容量是2的n次方，有利于提高计算元素存放位置时的效率，也降低了`hash`冲突的几率。因此，我们使用`HashMap`存储大量数据的时候，最好先预先指定容器的大小为2的n次方，即使我们不指定为2的n次方，`HashMap`也会把容器的大小设置成最接近设置数的2的n次方，如，设置`HashMap`的大小为 7 ，则`HashMap`会将容器大小设置成最接近7的一个2的n次方数，此值为 8 。
5. `HashMap`的负载因子表示哈希表空间的使用程度（或者说是哈希表空间的利用率）。当负载因子越大，则`HashMap`的装载程度就越高。也就是能容纳更多的元素，元素多了，发生`hash`碰撞的几率就会加大，从而链表就会拉长，此时的查询效率就会降低。当负载因子越小，则链表中的数据量就越稀疏，此时会对空间造成浪费，但是此时查询效率高。
6. `HashMap`不是线程安全的，`Hashtable`则是线程安全的。但`Hashtable`是一个遗留容器，如果我们不需要线程同步，则建议使用`HashMap`，如果需要线程同步，则建议使用`ConcurrentHashMap`。
7. 在多线程下操作`HashMap`，由于存在扩容机制，当`HashMap`调用`resize()`进行自动扩容时，可能会导致死循环的发生。
8. 我们在使用`HashMap`时，最好选择不可变对象作为`key`。例如`String`，`Integer`等不可变类型作为`key`是非常明智的。

- 由于最近工作较忙，也有拖延症发作的问题，所以文章迟迟未能完成发布，现时完成的文章其实对我而言，也不算太好，但还是打算先发出来让大家看看，一起学习学习，看有什么不好的地方，我再慢慢改进，如果此文对你有帮助，请给个赞，谢谢大家。

# 参考文章

[Java 8系列之重新认识HashMap](https://link.jianshu.com?t=https://tech.meituan.com/java-hashmap.html)
 [JDK 源码中 HashMap 的 hash 方法原理是什么？](https://link.jianshu.com?t=https://www.zhihu.com/question/20733617)
 [深入理解HashMap](https://link.jianshu.com?t=http://annegu.iteye.com/blog/539465)
 [HashMap负载因子](https://link.jianshu.com?t=http://www.cnblogs.com/yesiamhere/p/6653135.html)
 [Hashtable源码剖析](https://link.jianshu.com?t=http://blog.csdn.net/chdjj/article/details/38581035)
 [危险！在HashMap中将可变对象用作Key](https://link.jianshu.com?t=http://www.importnew.com/13384.html)
 [谈谈HashMap线程不安全的体现](https://link.jianshu.com?t=http://www.importnew.com/22011.html)



作者：liangzzz
链接：https://www.jianshu.com/p/32f67f9e71b5
来源：简书
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。