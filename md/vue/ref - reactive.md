![](https://cdn.nlark.com/yuque/0/2024/png/746419/1733816028427-67206401-5e08-4a3d-a525-294df2ecdf59.png)



```plain
const count = ref(0)
const state = reactive({
  count
})

console.log(state.count) // 0

state.count = 1
console.log(count.value) // 1

const otherCount = ref(2)

state.count = otherCount.value
console.log(state.count) // 2
// 原始 ref 现在已经和 state.count 失去联系
console.log(count.value) // 1

```





