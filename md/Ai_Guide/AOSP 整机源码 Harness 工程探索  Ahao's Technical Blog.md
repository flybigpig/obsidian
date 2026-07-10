## [AOSP 整机源码 Harness 工程探索](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#aosp-%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81-harness-%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2)

> 我们在一棵纯 AOSP 17（`android-17.0.0_r1`）整机源码树上，围绕 Claude Code 搭建了一套四层 harness 工程，让 coding agent 能够在上千个 git project、千万行量级的 repo 工程里稳定地**导航、编译、部署、验证**，目标设备是 Cuttlefish 虚拟机（`aosp_cf_x86_64_phone`）。本文先分析 coding agent 在整机源码树上应用的困难，再梳理 Anthropic 官方博客给出的大库通用要点，接着盘点网络上已有的同类方案，最后介绍我们的四层解决方案——每层解决什么问题、如何协同运转，以及一路探索踩过的坑。

___

## [一、问题：为什么 coding agent 在整机源码树上"开箱不可用"](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%B8%80%E3%80%81%E9%97%AE%E9%A2%98-%E4%B8%BA%E4%BB%80%E4%B9%88-coding-agent-%E5%9C%A8%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81%E6%A0%91%E4%B8%8A-%E5%BC%80%E7%AE%B1%E4%B8%8D%E5%8F%AF%E7%94%A8)

Claude Code 这类 agent 不给代码库建索引（不做 RAG），而是用 **agentic search** 现场导航——grep、读文件、跟引用，像一个新来的工程师翻代码。这个设计有一个巨大的好处和一个巨大的代价：

-   **好处：永不过期。** 它永远读的是 live 代码，不会像向量索引那样"返回两周前已改名的函数、或引用一个已删除的模块，却不告诉你它过期了"。
-   **代价：上下文窗口是唯一稀缺资源。** 导航的每一步（grep 结果、读过的文件）都在消耗上下文；导航质量完全取决于你把代码库"布置"得多好。

AOSP 整机源码树把这个矛盾推到极端，有三个放大器：

AOSP 整机树的三个放大器

Claude Code 的工作方式 — agentic search

不建索引 / 不做 RAG

现场导航  
grep、读文件、跟引用

好处  
永远读 live 代码，不会过期

代价  
每一步都消耗上下文窗口

规模  
上千 git project、千万行代码  
仅 external/ 就数百子目录

多语言  
C++/Java/Kotlin/AIDL/Rust 混编  
同名符号成海（无数个 onTransact）

编译重  
Soong+Kati+Ninja，单编十几分钟起步  
没有 npm test 式快速反馈

矛盾焦点  
上下文是唯一稀缺资源

结论  
必须先做 Harness 工程  
再谈让 agent 干活

不搭 harness，直接在整机树上用 coding agent 会反复撞上这些墙（均为实际遇到或验证过的）：

| 痛点 | 现象 |
|-------------|---------------------------------------------|
| 导航失效 | 全树 grep 一次就吞光上下文；文本匹配跳到错误的同名符号 |
| 上下文盲区 | 每个新会话都不知道"当前在做哪个 feature、哪些仓能动、有哪些硬约束" |
| 流程知识丢失 | 每次都要重新教它怎么编译、产物在哪、push 哪些文件 |
| "编过 = 改对"幻觉 | 编译成功就认为功能正确，会话心满意足地结束，设备一跑就崩 |
| 危险操作无门禁 | adb push/reboot、repo sync 这类动设备/动树的操作只靠模型自觉 |
| 知识污染 | 上下文文档混进 gerrit project 的提交 |
| 隐性经验反复付学费 | "改了这个类布局必须连某个 so 一起重编"这类血泪知识，不固化下来每次重新踩 |

**核心论断：模型不是瓶颈，环境才是。** 社区先行者 utzcoz 用 Claude Code 做成 4 个 AOSP 级项目后的结论也是如此：这类工作 coding agent 开箱做不好，能做成靠的是围绕 agent 搭的 **harness engineering**——"harness 诚实，产出就诚实"。

___

## [二、官方参照：Anthropic《How Claude Code works in large codebases》的要点](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%BA%8C%E3%80%81%E5%AE%98%E6%96%B9%E5%8F%82%E7%85%A7-anthropic%E3%80%8Ahow-claude-code-works-in-large-codebases%E3%80%8B%E7%9A%84%E8%A6%81%E7%82%B9)

上一节的困境不是 AOSP 独有的。Anthropic 官方博客《How Claude Code works in large codebases》归纳了 Claude Code 在大型代码库（百万行 monorepo、几十年遗留系统、几十个仓的分布式架构）落地成功的共性模式——它不是为 AOSP 写的，但几乎每一条都能对上整机树的处境，是我们方案的通用理论底座。这里先把它的要点提炼出来；后文第四节起的四层，本质就是把这些通用原则一条条落到整机树上的具体形态。

### [2.1 Claude Code 怎么在大库里导航：agentic search，而非 RAG](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_2-1-claude-code-%E6%80%8E%E4%B9%88%E5%9C%A8%E5%A4%A7%E5%BA%93%E9%87%8C%E5%AF%BC%E8%88%AA-agentic-search-%E8%80%8C%E9%9D%9E-rag)

博客把第一节我们已经点到的机制说得更透：

-   **agentic search**：像工程师一样遍历文件系统、读文件、用 grep 精确定位、跟着引用跨库跳转；**本地运行、不需要建立/维护/上传任何索引**。
-   **RAG 在大规模下会失效**：embedding 管线追不上活跃的工程团队——开发者查询时，索引反映的是几周/几天/几小时前的代码，于是**返回一个两周前已改名的函数、或引用一个上个 sprint 已删除的模块，却完全不提示它已过期**。
-   **代价与甜区**：agentic search 反过来要求**足够的起始上下文**才知道去哪找；导航质量取决于代码库被"布置"得多好（用 CLAUDE.md + skills 分层）。若向一个十亿行的库问一个模糊 pattern，会在开工前就撞上上下文窗口墙。**在代码库布置上投入的团队，效果明显更好。**

### [2.2 harness 与模型同等重要：五个扩展点 + 两项能力](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_2-2-harness-%E4%B8%8E%E6%A8%A1%E5%9E%8B%E5%90%8C%E7%AD%89%E9%87%8D%E8%A6%81-%E4%BA%94%E4%B8%AA%E6%89%A9%E5%B1%95%E7%82%B9-%E4%B8%A4%E9%A1%B9%E8%83%BD%E5%8A%9B)

博客点名一个最常见的误解——以为 Claude Code 的能力只由所用模型决定。实际上**围绕模型的生态（harness）比模型本身更决定表现**。harness 由五个扩展点构成，外加两项能力，且**叠加有顺序——每一层都建立在前一层之上**：

| 组件 | 是什么 | 何时加载 | 最适合 | 常见误用 |
|-------------|---------------------|---------|-----------------------------------|---------------------|
| **CLAUDE.md** | 自动读取的上下文文件 | 每个会话 | 项目约定、代码库知识（根=全局、子目录=局部） | 把该进 skill 的可复用经验塞进来 |
| **hooks** | 关键时刻运行的脚本 | 事件触发 | 自动化一致行为、**捕获会话经验（自我改进）** | 用 prompt 去做本应自动跑的事 |
| **skills** | 针对特定任务打包的指令 | 按需、相关时 | 跨会话/项目的可复用专长（渐进式披露、可按 path scope） | 全塞进 CLAUDE.md |
| **plugins** | 打包 skills/hooks/MCP | 配好后常驻可用 | 把一套可用配置分发到全组织 | 让好做法停留在部落知识 |
| **LSP** | 语言服务器的实时代码智能 | 配好后常驻可用 | 符号级导航、类型语言里自动查错 | 以为它自动就有 |
| **MCP servers** | 连接外部工具与数据 | 配好后常驻可用 | 让 Claude 够到本来够不到的内部工具 | 基础没跑通就先建 MCP |
| **subagents** | 独立上下文的隔离实例 | 被调用时 | **把探索与编辑分离**、并行 | 在同一会话里既探索又编辑 |

其中几条博客特别强调的：**hooks 最有价值的用法不是防错，而是让配置自我改进**（stop hook 会话末反思→提议改 CLAUDE.md）；**skills 可 path-scoped**，只在相关目录激活；**LSP 是多语言大库里最高杠杆的投资之一**（没有它，Claude 对文本 pattern-match，会落到错误的同名符号）；**subagents 的典型用法是只读子代理测绘子系统、写进文件，主代理再带全貌编辑**。

### [2.3 三个配置模式](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_2-3-%E4%B8%89%E4%B8%AA%E9%85%8D%E7%BD%AE%E6%A8%A1%E5%BC%8F)

博客从成功部署里提炼出三个反复出现的模式：

1.  **让大库对 Claude 可读**：CLAUDE.md 精简且分层（根只放指针 + 关键坑）；**在子目录而非仓根初始化**（Claude 会自动向上加载沿途每个 CLAUDE.md，根上下文不丢）；按子目录 scope test/lint 命令（跑全套会超时、烧上下文）；用 `.ignore` / 版本化的 `permissions.deny` 排除生成物、构建产物、三方码；目录结构不给力时写一份轻量 **codebase map**；跑 **LSP 让按符号而非字符串搜**。
2.  **随模型演进主动维护 CLAUDE.md**：为旧模型缺陷写的规则会拖累新模型（如"每次重构拆成单文件改动"会阻止新模型做它本已擅长的协调跨文件编辑）；为补模型/工具缺陷写的 skill/hook 一旦缺陷消失就成负担。**每 3–6 个月、或模型换代后感觉见顶时**做一次配置重审。
3.  **指派 owner**：技术配置本身不驱动采纳；铺开最快的组织在放开前就有专人/小队把工具接进工作流，出现 **agent manager**（PM/工程混合角色）或至少一个 **DRI**，并尽早对齐治理（谁管 skill/plugin、避免重复造轮子、AI 代码走同样的 review）。

### [2.4 适用边界](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_2-4-%E9%80%82%E7%94%A8%E8%BE%B9%E7%95%8C)

博客最后划了适用范围：Claude Code 面向**常规软件工程环境**——工程师是主要贡献者、用 Git、标准目录结构。非常规设置（游戏引擎的大二进制资产、非常规版本控制、非工程师贡献代码）需要额外的配置工作。

> 这些都是通用结论；而整机树把每一条的难度都放大了（第一节的三个放大器）。后文第四节起，就是我们把这套通用原则逐条落到 AOSP 整机树上的具体形态——并在两处（子目录初始化、plugin 分发）因 repo 工程的现实做了有意背离（见第四节末）。

___

## [三、他山之石：网络上已有的类似方案](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%B8%89%E3%80%81%E4%BB%96%E5%B1%B1%E4%B9%8B%E7%9F%B3-%E7%BD%91%E7%BB%9C%E4%B8%8A%E5%B7%B2%E6%9C%89%E7%9A%84%E7%B1%BB%E4%BC%BC%E6%96%B9%E6%A1%88)

动手自建之前，我们先调研了社区在"AOSP + coding agent"这个方向上已有的探索（调研时间 2026-07）。有代表性的四个方案，恰好各占一个生态位：

| 方案 | 形态 | 一句话定位 |
|--------------------------------------------------|-----------------|------------------------------------------------------------|
| [Lightrion AOSP RAG](https://lightrion.com/docs) | 托管 MCP 服务（SaaS） | 对公开 AOSP 各版本做语义检索，agent 即插即查 |
| [utzcoz《Using Claude Code on AOSP-scale projects》](https://utzcoz.github.io/2026/04/26/using-claude-code-on-aosp-scale-projects.html) | 方法论（博客） | 4 个已交付 AOSP 级项目沉淀的 harness engineering 十模式 |
| [hyperb1iss/hyperdroid-skill](https://github.com/hyperb1iss/hyperdroid-skill) | Claude Code 插件 | Android 通用领域技能包（adb/fastboot/构建/LineageOS）+ crash 分析 agent |
| [jonaschen/Android-Software](https://github.com/jonaschen/Android-Software) | 分层 skill 知识包 | L1 路由 → L2 子系统专家，防幻觉路径与跨域错配 |

### [Lightrion AOSP RAG：托管的 AOSP 语义检索](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#lightrion-aosp-rag-%E6%89%98%E7%AE%A1%E7%9A%84-aosp-%E8%AF%AD%E4%B9%89%E6%A3%80%E7%B4%A2)

一个商业 MCP 服务：把 AOSP 13–17 各发布版预先建好索引，暴露 `search_code` / `get_chunk` / `get_file` / `list_versions` / `diff_versions` 五个工具，任何 MCP 客户端加一个 bearer token 就能用自然语言检索 AOSP 源码，还能按 minor release 钉住版本、跨版本 diff。

-   **优点**：零本地成本——不需要本地源码树、不需要自己建索引；多版本覆盖 + 跨版本 diff 是独有能力（"这个函数在 15→17 之间改了什么"一问即得）；接入是标准 MCP，五分钟配完。
-   **局限**：它索引的是**公开 AOSP 发布版**，而整机开发的工作对象是**自己的本地树**——你刚改过的代码、本地 feature 分支、树上的任何 delta 它都不知道。这正是 Anthropic 官方博客点名的 RAG 死穴（"返回已改名的函数却不告诉你它过期了"）在 fork 场景下的极端形态。此外代码问题出网查询有保密性顾虑；且它只覆盖"读与查"这一层，编译、部署、验证、护栏全不涉及。
-   **我们的取舍**：不能作为本地树的主导航（live 树必须 agentic search + LSP），但作为"查上游基线/跨版本差异"的补充通道有真实价值。

### [utzcoz 十模式：被四个交付项目验证过的方法论](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#utzcoz-%E5%8D%81%E6%A8%A1%E5%BC%8F-%E8%A2%AB%E5%9B%9B%E4%B8%AA%E4%BA%A4%E4%BB%98%E9%A1%B9%E7%9B%AE%E9%AA%8C%E8%AF%81%E8%BF%87%E7%9A%84%E6%96%B9%E6%B3%95%E8%AE%BA)

作者用 Claude Code 做成并发布了 4 个 AOSP 级项目（ARM64→x86\_64 二进制翻译器、AOSP 14 多窗口补丁集、Chromium WebXR 移植、64 章 AOSP 内核书），沉淀出十条 harness engineering 模式，分三组：跨会话保存状态（CLAUDE.md 行为契约、handoff 交接文档）、验证（模拟器基座、verify 脚本只编码一次、红条 TDD、读截图判对错）、输出可信（结论带源码路径行号、冷启动对抗 review）。

-   **优点**：唯一经过"真的交付了东西"检验的完整方法论；对"编过 = 改对"幻觉、"似是而非 ≠ 正确"这两个 agent 根性问题给出了系统解法；多条模式可直接照抄（CLAUDE.md 只写 agent 默认会犯的错、verify 脚本单入口确定性输出）。
-   **局限**：它是方法论而非可安装工件，每个项目都要自己重新落地；其项目形态是"单仓 fork + 模拟器"，没有处理 repo 千仓工程特有的问题——上下文文件会污染 gerrit project、上下文如何随 feature 分支切换；对代码智能层（LSP/compdb）也没有展开。
-   **我们的取舍**：十模式是本文方案在理念层的最大来源——CLAUDE.md 行为契约、verify 确定性脚本、"harness 诚实产出就诚实"都直接进入了设计；repo 工程特有的部分（第六、七节）则是我们补上的。

### [hyperdroid-skill：插件化的 Android 通用技能包](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#hyperdroid-skill-%E6%8F%92%E4%BB%B6%E5%8C%96%E7%9A%84-android-%E9%80%9A%E7%94%A8%E6%8A%80%E8%83%BD%E5%8C%85)

LineageOS 社区开发者的 Claude Code 插件（MIT）：四个按触发词自动激活的 skill（`android` 设备/adb、`android-fastboot` 刷机/分区/防砖、`android-build` Gradle+AOSP 构建、`lineageos` repo/Gerrit 工作流）加一个受限工具的 `crash-analyzer` 子代理（自主收集 logcat/tombstone/ANR 后给诊断）。

-   **优点**：**工程骨架是四家里最值得抄的**——单仓分发多 skill + agent（plugin.json/marketplace 一键装）、渐进式披露（SKILL.md 速查 + `references/` 深度页按需加载）、触发词自动激活、受限工具 + 固定工作流的子代理模板、仓库自校验 Makefile（CI 强制每个 skill 结构合规）。
-   **局限**：内容是**参考手册级的通用知识**（adb/fastboot/构建命令速查），视角偏三方 ROM 玩机而非整机平台开发；不绑定任何具体源码树——不知道你的 feature、你的编译产物、你的验证脚本；对树内导航、上下文经济、gerrit 污染等核心矛盾不涉及。
-   **我们的取舍**：抄骨架、自己填肉——渐进式披露和触发激活的思想直接体现在我们的 path-scoped skill 设计里（`paths` glob 替代触发词，粒度更准）。

### [Android-Software：分层专家路由的知识包](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#android-software-%E5%88%86%E5%B1%82%E4%B8%93%E5%AE%B6%E8%B7%AF%E7%94%B1%E7%9A%84%E7%9F%A5%E8%AF%86%E5%8C%85)

面向 Android Software Owner / BSP 工程师的分层 skill 集（Beta，对 Android 15 验证）：所有任务先过 L1 路由器（意图 → 已验证的 AOSP 路径映射），再加载对应的 L2 子系统专家（build/SELinux/HAL/framework/init/内核 GKI/bootloader/ATF/pKVM 等 12 个），每个专家带子系统知识、禁止动作和工具链；另有 hindsight notes 机制沉淀跨会话经验。

-   **优点**：直击 agent 在 AOSP 上的三大失败模式（幻觉路径、跨域错配——把 bootloader 问题路由给 init、版本知识漂移）；"MMU 式按需加载"与我们的上下文经济诉求同源；子系统覆盖面最广（连 LK/ATF/pKVM 这类 vendor 层都有路由位）；没有本地源码也能回答问题。
-   **局限**：本质是**静态知识包**——与"你这棵树"零绑定，答案来自预写的知识而非现场读码，版本演进要人工维护（对 A15 验证，用在 17 上就有漂移窗口，恰是它自己要解决的问题）；无 symbol 级导航；无编译→部署→验证闭环；单人 Beta 项目，成熟度有限。
-   **我们的取舍**：分层按需加载的思想与我们"索引粒度注入、详情按需加载"殊途同归；但我们把"知识从哪来"反过来了——不预写知识，让 agent 现场读真代码，harness 只负责把它引到对的地方。

### [四方案对照与我们的位置](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%9B%9B%E6%96%B9%E6%A1%88%E5%AF%B9%E7%85%A7%E4%B8%8E%E6%88%91%E4%BB%AC%E7%9A%84%E4%BD%8D%E7%BD%AE)

用整机树内开发需要的四类能力——代码智能、上下文、流程、护栏与验证，这也正是下文我们方案的四层框架——给它们做覆盖度体检（● 深度覆盖 ◐ 部分/通用级 ○ 基本不涉及）：

| 方案 | ① 代码智能 | ② 上下文 | ③ 流程 | ④ 护栏与验证 |
|--------------------|----------------------|--------------------------|---------------------|---------------------------|
| Lightrion AOSP RAG | ◐ 仅上游版本，不含本地改动 | ○ | ○ | ○ |
| utzcoz 十模式 | ○ 未展开 | ◐ CLAUDE.md 契约 + handoff | ◐ 脚本化约定 | ● verify/红条 TDD/对抗 review |
| hyperdroid-skill | ○ | ○ | ◐ 通用命令速查 | ◐ crash-analyzer |
| Android-Software | ○ 防幻觉路径 ≠ symbol 导航 | ◐ 分层按需加载 | ◐ 子系统流程知识 | ○ |
| **本文方案** | ● 本树 compdb + clangd | ● 随 feature 分支切换（单文件软链） | ● path-scoped skill | ● 硬门禁 + verify 闭环 |

本文四层方案utzcoz 十模式Android-Softwarehyperdroid-skillLightrion AOSP RAG通用 AOSP 知识绑定本地源码树单点能力全流程闭环社区方案定位 — 知识通用性 vs 与本地树的集成深度

结论一目了然：四个方案分别解决了检索、方法论、通用领域知识、知识路由，**但没有一个解决"这一棵树"的问题**——本地 fork 的 symbol 级实时导航、repo/gerrit 布局下不污染上游的上下文组织、随 feature 分支自动切换的工作状态、绑定本树目标设备的确定性验证环。这块空白，就是下文四层 harness 的主体；而各家的长处（utzcoz 的验证纪律、hyperdroid 的渐进披露骨架、Android-Software 的按需分层思想）都被吸收进了对应层的设计。

___

## [四、方案总览：四层 Harness](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%9B%9B%E3%80%81%E6%96%B9%E6%A1%88%E6%80%BB%E8%A7%88-%E5%9B%9B%E5%B1%82-harness)

### [4.1 一个业务前提与五个术语](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_4-1-%E4%B8%80%E4%B8%AA%E4%B8%9A%E5%8A%A1%E5%89%8D%E6%8F%90%E4%B8%8E%E4%BA%94%E4%B8%AA%E6%9C%AF%E8%AF%AD)

展开四层之前，先厘清本方案赖以成立的一个业务前提和几个反复出现的术语——它们决定了这套 harness 为什么可行、四层各自靠什么落地。

**业务前提：一个专项 = 一个 feature = 一个本地分支 = 3–8 个单仓。** 真实的手机厂商以**专项**的形式推进 OS 特性开发（一个专项就是一次成体系的特性迭代）。我们把一个专项对应成一个 **feature**，并约定 **一个 feature 独占一个 repo 本地分支**（`repo start <feature> --all`），该 feature 的全部改动只在这个分支上进行——这样 harness 才有一个稳定的"当前在做什么"的锚点。一个 feature 通常只触及 **3–8 个 git 单仓**（例如"新增一个系统服务 + 一个边栏应用"，落在 `frameworks/base`、`frameworks/native`、新建 app 仓、`build/make`、`system/sepolicy` 上）。正是"**涉及仓有限、且随分支固定**"这个业务事实，让后文所有"随 feature 组织、随分支自动切换、按仓精简"的机制得以成立。

**五个术语（四层各自的关键落地物，正文表格里会直接用到）：**

-   **LSP（Language Server Protocol，语言服务器协议）**：编辑器/agent 与语言服务器之间的标准协议，把"跳到定义、查找所有引用、按符号导航、类型报错"这类**代码智能**能力暴露出来。它让 agent 按 **symbol（符号）** 而非文本字符串检索——同名函数在不同文件/语言里能被精确区分，不会 pattern-match 到错误的符号。C++ 侧我们用 **clangd** 作为语言服务器，喂给它一份 **compdb**（`compile_commands.json`，编译命令数据库）即可（第①层，详见第五节）。
-   **SessionStart hook**：Claude Code 在会话启动 / 恢复 / 清屏 / 压缩时触发的钩子脚本。我们用它在每次会话"睁眼"时，按当前分支**把树根 `CLAUDE.md` 软链重指到 `features/<分支>/CLAUDE.md`**——于是 Claude Code 加载根 `CLAUDE.md` 时，就把这个 feature 的全部上下文（在做什么、涉及哪些仓、各仓约定、验证入口）随之载入**持久上下文**（第②层，详见第六节）。
-   **path-scoped skill**：skill 是**按需加载**的打包指令；带 `paths` glob 的 skill 只在 agent 读到匹配路径的代码时才激活。我们用它承载"改到这片代码怎么编译 / push / 验证"这类过程性知识——不读到就零上下文占用（第③层，详见第七节）。
-   **`permissions.ask` 硬门禁**：Claude Code 的权限机制，把匹配到的危险命令（`adb push/reboot`、`cvd start/stop`、`repo sync`、`m clean` 等）变成执行前的**系统级弹窗确认**，不依赖模型记性（第④层护栏，详见第八节）。
-   **`features/<分支>/verify-*.sh` 确定性脚本**：每个 feature 自带的验证脚本，就是这个 feature 的"测试"——输出只有确定性的 **PASS/FAIL**，用来斩断"编过 = 改对"的幻觉（第④层验证，详见第八节）。

### [4.2 四层总览](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_4-2-%E5%9B%9B%E5%B1%82%E6%80%BB%E8%A7%88)

我们把「导航、上下文、流程、护栏与验证」四件事做成工程化的基础设施，让 agent 在这棵树上的每次会话都站在同一套地基上：

| 层 | 解决什么 | 落地物 |
|---------|------------------------|------------------------------------------------------------------------------------------------------------|
| ① 代码智能 | 按 symbol 而非文本导航 | 树根 `.clangd` → feature 精简 compdb（C++）；Java/Kotlin 明确不配 LSP |
| ② 上下文 | 每个会话自动知道"在哪、做什么、什么不能碰" | 根 `CLAUDE.md` 软链到 `features/<分支>/CLAUDE.md`（单文件：树级 bootstrap/硬约束 + 该 feature 全部上下文）+ SessionStart hook 按分支重指软链 |
| ③ 流程 | 动到哪片代码就知道怎么编译/push/验证 | 树根 `.claude/skills/` 若干 path-scoped skill |
| ④ 护栏与验证 | 防误操作设备、斩断"编过=改对" | `permissions.ask` 硬门禁 + `features/<分支>/verify-*.sh` 确定性脚本 |

> **与官方博客扩展点框架的对应关系**（博客要点见第二节）：博客把 harness 拆成五个扩展点（CLAUDE.md、hooks、skills、plugins、MCP servers）外加两项能力（LSP、subagents），并强调**叠加有顺序——每一层都建立在前一层之上**。我们这张"四类能力"表是同一套东西按"解决什么问题"重排的：① 代码智能 = **LSP**；② 上下文 = **CLAUDE.md + hooks**（SessionStart 动态注入）；③ 流程 = **skills**（path-scoped）；④ 护栏 = **hooks**（permissions/校验）+ verify 脚本。博客说的叠加顺序在我们的探索历程（第十节）里也如实复现：先用 CLAUDE.md 定行为契约、再用 hook 注入上下文、再用 skill 承载流程、最后补护栏与验证——每一战都踩在上一战的地基上。**五个扩展点里我们只暂缓了两个（plugins 与 MCP servers），另有两处对博客通用建议的有意背离，理由见本节末。**

四层全部落在**树根的游离文件**上。这里有一个 repo 工程的关键事实：**源码树根不是 git 仓**（树根只有 `.repo/`，没有 `.git/`），所以放在树根的文件不被任何 gerrit project 跟踪，soong/kati 也不会把纯 markdown 目录当模块编进整机——这是整套方案"不污染上游"的地基。

```bash
<AOSP_ROOT>/                          # repo 工程根（非 git 仓）
├── CLAUDE.md                         # ② 软链 → features/<分支>/CLAUDE.md（hook 按分支重指）
├── .clangd                           # ① 指向 feature 精简 compdb（绝对路径）
├── compile_commands.json             # ① 根符号链
├── gen-compdb-clangd.sh              # ① compdb 两段式刷新脚本
├── .claude/
│   ├── settings.json                 # ② hooks 注册 + ④ permissions 门禁
│   ├── hooks/load-feature.sh         # ② 按分支把树根 CLAUDE.md 软链重指到 features/<分支>/CLAUDE.md
│   ├── hooks/check-branch-drift.sh   # ② 会话中途切分支告警
│   ├── hooks/compdb-stale-nudge.sh   # ① compdb 时效补漏（PostToolUse：repo sync/新源文件/新 bp）
│   ├── rules/compdb-freshness.md     # ① compdb 时效提醒（path-scoped rule：读到构建文件时）
│   └── skills/build-*/SKILL.md       # ③ path-scoped 编译/验证 skill
└── features/                         # ② 独立 git 仓（不在 manifest，repo/gerrit/soong 全不可见）
    └── dev-sidebar/                  # 目录名 = repo 分支名 = feature 名
        ├── CLAUDE.md                 # ② 该 feature 的【单文件全部上下文】：树级 bootstrap/硬约束 + 总览 + 各仓约定
        ├── repos.tsv                 # ① compdb 仓集单一事实源（gen-compdb 据它取标 compdb 的仓）
        ├── check-branch.sh           # 涉及仓分支一致性检查
        └── verify-sidebar.sh         # ④ 确定性验证脚本
```

> **📦 可跑 Demo**：本节这棵目录树的最小可运行复刻见同级 [`aosp-harness-demo/`](https://github.com/yuandaimaahao/aosp-harness-demo)——四层落地物一应俱全，关键脚本都带 `--demo`，不需要真实 AOSP 树，`./run-demo.sh` 即可一键演示四层如何协同。下文五~八节每节开头的「▶ Demo」标注，就指向它对应的文件。

**对博客扩展点的三点取舍（都是被 repo/整机树的现实逼出来的，不是遗漏）：**

-   **背离一：博客建议"在子目录而非仓根初始化"**（让 agent scope 到与任务相关的部分，Claude 会自动向上加载沿途每个 CLAUDE.md，根上下文不丢）。这条建议的**两半我们最终都不采纳**，各有原因：
    -   **"从哪启动"这一半**——我们的 cwd **必须是树根**（envsetup / lunch / m / adb 全都要求树根 cwd），所以反其道而行。
    -   **"把 CLAUDE.md 放到子目录、按需加载"这一半**——我们**曾经用起来过**（早期方案：hook 为每个涉及仓在其仓根物化一份 `CLAUDE.md`、`.git/info/exclude` 隔离不进 gerrit、靠 Claude Code 子目录按需加载），但**后来主动放弃**。放弃的依据是一个实测发现：**子目录 CLAUDE.md 与 hook 的 stdout 注入一样，进的都是"会话消息流"，长会话触发上下文压缩时会被摘掉（易失），且不进子代理**（详见 6.2 的 v5→v6 演进）。于是最终形态（v6）把一个 feature 的**全部**上下文（树级约束 + 总览 + 各仓约定）内联进**单个** `features/<分支>/CLAUDE.md`，让树根 `CLAUDE.md` **软链**到它——根 CLAUDE.md 是 cwd 的 project memory，**启动即进 Memory files 持久桶、抗压缩、子代理也加载**。所以我们对博客这条的最终态度是：**两半都不照搬，改用"单文件软链根"这个更适配 repo 工程、又更抗易失的形态**（详见第六节）。
-   **背离二：博客把 plugins 当作"分发可复用配置、防止好做法停留在部落知识"的手段**（打包 skills/hooks/MCP，marketplace 一键装）。我们**刻意不把这套 harness 做成 plugin**——它的价值恰恰在于是一组**树根游离文件**（见上），做成 plugin 会脱离"树根非 git 仓"这个不污染上游的地基。跨机分发改由 `features/` 独立 git 仓推私有 remote 实现（第九节）。
-   **暂缓一：MCP servers**。博客点名一个常见错误——"基础还没跑通就先建 MCP 连接"。我们目前只在"查上游基线 / 跨版本 diff"这类**读侧**场景把 Lightrion 这类 MCP 当补充通道（第三节）；"把结构化检索暴露成 agent 可直接调用的工具"是后续可演进项，而非当前地基。

> 下文以一个真实工作中的 feature 为例（记作 `dev-sidebar`）：在 AOSP 17 上新增一个系统服务 + 一个常驻边栏应用，涉及 `frameworks/base`、`frameworks/native`、新建 app 仓、`build/make`、`system/sepolicy` 五个仓。

___

## [五、第①层 代码智能：让导航按 symbol，而不是按文本](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%BA%94%E3%80%81%E7%AC%AC1%E5%B1%82-%E4%BB%A3%E7%A0%81%E6%99%BA%E8%83%BD-%E8%AE%A9%E5%AF%BC%E8%88%AA%E6%8C%89-symbol-%E8%80%8C%E4%B8%8D%E6%98%AF%E6%8C%89%E6%96%87%E6%9C%AC)

> ▶ **Demo**：[`aosp-harness-demo`](https://github.com/yuandaimaahao/aosp-harness-demo) 里对应 `.clangd`、`gen-compdb-clangd.sh`（带 `--demo` 演示两段式过滤；无参时读 `features/dev-sidebar/repos.tsv` 里标 `compdb` 的仓）、`features/dev-sidebar/repos.tsv`、`.claude/rules/compdb-freshness.md`、`.claude/hooks/compdb-stale-nudge.sh`（读构建文件走 rule、不读构建文件的 staling 动作走 hook）。

这一层要做的事，一句话就是**给 agent 配好 LSP**——让它像 IDE 里的工程师那样按 symbol 导航，而不是对文本做 pattern-match。LSP 被 Anthropic 官方博客点名为多语言大库里"最高杠杆的投资之一"：没有它，agent 在命名相似的代码里会落到**错误的 symbol**（整机树里 `onTransact` 这类同名符号成海，是最大的导航陷阱）。下面按"**AOSP 提供了什么机制 → 我们怎么用 → Claude Code 侧怎么配 → 配好后 agent 怎么查代码 → 好处**"讲清 C++ 侧的落地；Java/Kotlin 侧论证后刻意不配（见 5.2）。

### [5.1 C++：clangd + 两段式 compdb](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_5-1-c-clangd-%E4%B8%A4%E6%AE%B5%E5%BC%8F-compdb)

**① AOSP 为 LSP 准备好的机制。** clangd（C/C++ 的语言服务器）要工作，只需喂它一份准确的 `compile_commands.json`（compdb，编译命令数据库——记录每个源文件用哪些 flag 编译）。AOSP 内建了生成它的机制：让 soong 在**分析阶段**顺带吐出全树 compdb（几分钟，不需要真编译）；树内还自带一份与构建工具链同版本的 clangd（`prebuilts/clang/host/linux-x86/clang-*/bin/clangd`）。

```ini
SOONG_GEN_COMPDB=1 m nothing
# → out/soong/development/ide/compdb/compile_commands.json
```

**② 我们怎么用：两段式精简。** 全树 compdb 对 clangd 太重（本树实测 **113,371 条 / 1.97GB**）。脚本 `gen-compdb-clangd.sh` 把它做成两段——soong 全树库只作**过滤源**，再按当前 feature 涉及的仓过滤出精简库；soong 版每文件已去重（同一 `.cpp` 的 shared/static/arch 变体只留一条），过滤后即净（用 `ninja -t compdb` 从构建图导出也行，但不去重，全树会膨胀到几十万条）：

SOONG\_GEN\_COMPDB=1 m nothing  
soong 分析阶段副产物，约 1 分钟

全树 compdb  
113,371 条 / 1.97GB

按 feature 涉及仓过滤  
gen-compdb-clangd.sh 两段式

compdb-feature/  
12,152 条 / 282MB  
含涉及仓的 out/.intermediates 生成源

树根 .clangd  
CompilationDatabase 写绝对路径

in-tree clangd  
与本树构建工具链同版本

按 symbol 精准跳转 / 找引用  
preamble 秒级，AST 完整

**②′ 仓集怎么和 feature 对齐：单一事实源，随分支自动取。** 上图"按 feature 涉及仓过滤"里的"涉及仓"从哪来？早期是写死在 `gen-compdb-clangd.sh` 里的一个 `REPOS` 数组、靠人肉与 `_index.md` 对齐——这恰恰犯了第七节要力戒的"同一份清单写两遍、必然漂移"：切了分支却忘了改脚本，compdb 仍是上一个 feature 的仓集，新仓文件静默地拿不到精确编译参数（脚本照样跑成功，是最难发现的那类错）。定型做法是把仓集也做成**随分支的单一事实源**，并进一步与第六节的涉及仓清单**合并成同一份**：每个 `features/<分支>/` 放一份机器可读的 `repos.tsv`，一行一个涉及仓、列出「仓路径 + 单仓约定文件 + 标签 + 说明」，其中标签 `compdb` 标记"该仓有 C++、要进 clangd"（`build/make`、`system/sepolicy` 无 C++ 不打此标签）。本节的 `gen-compdb-clangd.sh` 读它、**只取打了 `compdb` 标签的仓**作为过滤仓集。（v2 上下文层重构后，第六节的 `load-feature.sh` 已**不再消费** `repos.tsv`——feature 的涉及仓总览与各仓约定都内联进单个 `features/<分支>/CLAUDE.md`；`repos.tsv` 于是主要作为 compdb 仓集的机器可读清单存续，两脚本仍共用同一条"锚定仓读当前分支"的逻辑。）脚本无参时**复用第六节 hook 那条锚定仓链读出当前分支**（`frameworks/base → native → …`），再读该 feature 的 `repos.tsv`；传仓前缀参数则临时覆盖（一次性扩范围），清单缺失才回退脚本内兜底并告警。于是"换 feature"对 compdb 层也变成零手动——切分支即自动跟随，与第六节的上下文软链重指共用同一个"当前分支"锚点（清单不再共用——第六节 v2 走单文件 `CLAUDE.md`、不读 `repos.tsv`）。（这份 `repos.tsv` 一度是独立的 `compdb-repos.txt`，与第六节涉及仓清单各写一份，正是本段开头那种"两份必然漂移"的隐患；合并成一份、用一列 `compdb` 标签区分"要不要进 compdb"后，既消了重复，又不必去解析 `_index.md` 那张**给人读**的 markdown 表格——机器读的 `repos.tsv` 一行 `read` 即得，比表格解析稳得多，也顺带回答了"为什么不直接取 `_index.md` 的涉及仓表格"。）

**③ Claude Code 侧要做的配置。** 让 agent 用上这套，只需四件事：

-   树根放一个 `.clangd`，把 `CompilationDatabase` 指向精简库的**绝对路径**，并开后台索引：

```yaml
# 树根 .clangd
CompileFlags:
  CompilationDatabase: <AOSP_ROOT>/out/soong/development/ide/compdb-feature/
Index:
  Background: Build
```

-   Claude Code 通过 **LSP（plugin 层）** 把 clangd 挂进来（博客原话："LSP is accessed through the plugin layer"），agent 便获得符号级能力；
-   让它拉起的是**树内 prebuilt clangd**（与生成 compdb 的工具链同版本，避免 flag 兼容噪音）——用 `~/.local/bin/clangd` 的 shim 按 `$PWD` 分树分发，多树共存时防串台（见第十节踩坑二）；
-   从**树根**启动 Claude Code，让 workspace root 与 `.clangd`、compdb 的绝对路径对齐。

**④ 配好之后 agent 怎么查代码。** 不再靠全树 grep 猜同名符号，而是走 LSP 的符号级能力——跳到定义（go-to-definition）、查所有引用（find-all-references）、列工程符号、hover 看类型签名。例如追一个 binder 调用，能从接口方法**直接跳到具体实现**，而不必在几十个同名 `onTransact` 里逐个排除；clangd 后台已建好索引（`Index.Background: Build`），preamble 秒级、AST 完整；只改函数体不用重生成 compdb——clangd 实时读源文件内容。

**⑤ 好处。**

-   **精准**：按 symbol 区分同名符号，不落到错误符号——这是整机树最大的导航陷阱；
-   **省上下文**：LSP 直接返回定义/引用的确切位置，把"过滤"放在 agent 读文件**之前**，不用打开一堆文件人工判断哪个才对（正是博客强调的 context 经济性）；
-   **快**：秒级 preamble + 完整 AST + 后台增量索引；
-   **永不过期**：读的是 live 源文件，与 agentic search 同源的好处。

**⑥ 两条属于"配置"一部分的实测坑。**

-   **一律不要给 AOSP 的 clangd 加 `--query-driver`**。社区常见的这个参数在 AOSP 上是反作用：clangd 会"裸跑"驱动探测系统 include（不带 compdb 里的 `-nostdlibinc`/`-target` 全套参数），把宿主机 glibc 头以 `-isystem` 注入，与 bionic 头混装后爆出上百个 `__GLIBC_USE is not defined` 之类的错误。AOSP 的 compdb 命令本身已带全套 bionic `-isystem`，什么都不需要补。
    
-   **compdb 有时效，用「rule + hook」两条腿兜**：改了 `Android.bp`/`Android.mk`、`repo sync`、新增源文件后 compdb 就过期（只改函数体不用——clangd 实时读源文件）。两条腿按「动作读不读构建文件」分工：
    
    -   **读构建文件 → path-scoped rule** `.claude/rules/compdb-freshness.md`（`paths:` 命中 `**/Android.{bp,mk}`）：agent 一**读到**构建文件就自动注入提醒，零常驻成本、压缩后也只在下次读构建文件时才回来（恰好只在你要动构建结构时在场）；并顺带兜住 ②′ 的另一半——新模块建到 `repos.tsv` 之外的仓时，提醒补一行标 `compdb`（否则新仓文件静默拿不到精确编译参数，正是 ②′ 那类最难发现的错）。
    -   **不读构建文件 → PostToolUse hook** `.claude/hooks/compdb-stale-nudge.sh`：`repo sync`、新增 `.cpp`（srcs 用 glob 时不改 bp）、全新 Write 一个 `Android.bp`（新模块）这三类 staling 动作**不读任何构建文件**，rule 的 `paths` glob 够不着；改由这条 hook 在动作**发生后**回注一句提醒（`hookSpecificOutput.additionalContext`，官方确认对 PostToolUse 生效），温和不拦、命中才出声（Bash 侧用 `if: "Bash(repo sync:*)"` 预过滤，不给每条 bash 添负担）。
    
    一句话：**读构建文件走 rule，不读构建文件的 staling 动作走 hook**——两条腿拼出对 compdb 时效的完整覆盖，都不靠人记。
    

> **一个值得知道的上游坑**：AOSP 曾有一个"省内存"的临时 commit（`b790b9cb8`，2025-03，soong 核心作者所写），在每个 cc 模块构建动作结束就把 `compiler` 字段置 nil；而 compdb 生成器按设计在所有模块之后才遍历——遍历时七万多个 cc 模块的 compiler 全是 nil，收集到 0 条，`SOONG_GEN_COMPDB=1 m nothing` 静默产出一个空 `[]`。更新的 AOSP 基线已移除该 commit，但若你的基线恰好落在这个窗口，标准命令会"成功地什么都不生成"。检查方法：`grep 'c.compiler = nil' build/soong/cc/cc.go`；绕行方案是 `ninja -t compdb` 从 ninja 构建图提取，或给 `cc.go` 打三行补丁条件保留 compiler。

### [5.2 Java/Kotlin：论证之后，刻意不配](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_5-2-java-kotlin-%E8%AE%BA%E8%AF%81%E4%B9%8B%E5%90%8E-%E5%88%BB%E6%84%8F%E4%B8%8D%E9%85%8D)

Java 侧我们完整走了一遍 aidegen + jdtls（Eclipse JDT 语言服务器）路线，最终结论是**不配**，且放弃本身是重要的探索成果：

1.  **命脉工具已弃用**：aidegen 自己打印 "AIDEGen is no longer supported"，官方引导到 Android Studio for Platform——那是 GUI IDE，对 agent 毫无帮助。
2.  **架构性不匹配**：agent 的 LSP 以进程启动目录为 workspace root，而编译/adb 都要求从树根启动；jdtls 是 workspace 模型，会从根 URI **遍历整棵树**找工程——实测空转吃约 2GB 内存、`documentSymbol` 反复 internal error；同一时刻 clangd 对 `.cpp` 秒回。
3.  **无法收窄**：官方 jdtls 插件是薄封装，不暴露限制扫描范围的配置。

这一战沉淀出后续所有工具选型的**判据**：

> **能靠"精确文件清单"喂的 LSP（clangd 读 compdb，不遍历树）就配；要靠"遍历大树 workspace"、且工具链已弃用的就不配。**

Java/Kotlin 导航改用 Grep 搜符号 + Read，跨 Java↔JNI↔native 追踪时用 JNI 注册名（如 `android_view_*`）作 Grep 锚点。不硬配不是偷懒，是止损——后文"踩坑精选"里还有它写坏源码树的实证。

___

## [六、第②层 上下文：每个会话睁眼就知道"在哪、做什么、什么不能碰"](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%85%AD%E3%80%81%E7%AC%AC2%E5%B1%82-%E4%B8%8A%E4%B8%8B%E6%96%87-%E6%AF%8F%E4%B8%AA%E4%BC%9A%E8%AF%9D%E7%9D%81%E7%9C%BC%E5%B0%B1%E7%9F%A5%E9%81%93-%E5%9C%A8%E5%93%AA%E3%80%81%E5%81%9A%E4%BB%80%E4%B9%88%E3%80%81%E4%BB%80%E4%B9%88%E4%B8%8D%E8%83%BD%E7%A2%B0)

> ▶ **Demo**：[`aosp-harness-demo`](https://github.com/yuandaimaahao/aosp-harness-demo) 里对应 `CLAUDE.md`(软链)、`.claude/settings.json`、`.claude/hooks/{load-feature,check-branch-drift}.sh`、`features/dev-sidebar/{CLAUDE.md,repos.tsv,check-branch.sh}`；`load-feature.sh` 把树根 `CLAUDE.md` 软链重指到 `features/dev-sidebar/CLAUDE.md`（单文件全部上下文）。demo 用树根 `CURRENT_FEATURE` 模拟"锚定仓当前分支"。

**这一层做的事一句话说清：让 agent 每次"睁眼"就拿到当前 feature 的上下文——在做哪个 feature、能动哪些仓、每个仓有什么约定、什么不能碰——无需人工每次交代，且长会话里也不丢。** 做法极简到只剩一个文件：**一个 feature 的全部上下文都在 `features/<分支>/CLAUDE.md` 一个文件里**（树级约束 + feature 总览 + 各仓约定全内联）；树根 `CLAUDE.md` 是**指向它的软链**，SessionStart hook 按当前分支重指软链。因为树根 `CLAUDE.md` 是 cwd 的 project memory，**启动即整份进 Memory files 持久桶**——抗上下文压缩、子代理也加载。

承接 4.1 的前提（一个 feature = 一个 repo 本地分支 = 3–8 个单仓），本层的四条需求是：**按 feature 组织、随分支自动切换、不污染 gerrit、上下文持久不丢**。下文先给出方案的**具体构成与端到端流程**（6.1），再解释那个逼出整套设计的 git 语义死结与破局（6.2），最后讲几个关键设计决策、以及从"物化各仓 CLAUDE.md"到"单文件软链"的演进（6.3）。

### [6.1 方案概览：由哪些文件构成、启动时怎么跑](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_6-1-%E6%96%B9%E6%A1%88%E6%A6%82%E8%A7%88-%E7%94%B1%E5%93%AA%E4%BA%9B%E6%96%87%E4%BB%B6%E6%9E%84%E6%88%90%E3%80%81%E5%90%AF%E5%8A%A8%E6%97%B6%E6%80%8E%E4%B9%88%E8%B7%91)

先看**具体由哪些东西构成**（都放在树根，不进任何 gerrit 仓）：

-   **`features/` —— 一个放在树根的独立 git 仓**：每个 feature 一个子目录，**目录名就等于该 feature 的 repo 分支名**。以 `dev-sidebar` 为例：

```bash
features/
└── dev-sidebar/                # 目录名 = 分支名 = feature 名
    ├── CLAUDE.md               # 【该 feature 的全部上下文，单文件】：树级约束 + 总览 + 各仓约定；树根 CLAUDE.md 软链到它
    ├── repos.tsv               # compdb 仓集单一事实源（gen-compdb 据它取标 compdb 的仓）
    ├── check-branch.sh         # 涉及仓分支一致性检查
    └── verify-sidebar.sh       # 确定性验证脚本
```

-   **`repos.tsv` —— compdb 仓集的单一事实源**：一行一个涉及仓，四列——仓路径、单仓约定文件（v2 已不再据此物化，留列作说明/历史）、标签（含 `compdb` = 有 C++、要进 clangd）、一句说明。v2 里它主要喂第①层的 `gen-compdb-clangd.sh`（据 `compdb` 标签取过滤仓集）；上下文层不再消费它（feature 的涉及仓总览已写进单文件 `CLAUDE.md`）。

```bash
frameworks/base           frameworks-base.md    compdb  SidebarService + SystemServer 注册
frameworks/native         frameworks-native.md  compdb  SidebarFlinger（native 合成侧）
packages/apps/SidebarApp  -                     compdb  常驻边栏 app（编译/push 走 skill）
build/make                -                     -       产品配置接入新模块
system/sepolicy           -                     -       新服务 SELinux 策略
```

-   **`CLAUDE.md` —— 该 feature 的单文件全部上下文**（树根 `CLAUDE.md` 软链的目标）：一份手写文件，含三段——树级约束（bootstrap / 硬约束 / 代码导航，只写 agent 默认会犯的错）、feature 总览（目标 + 涉及仓 + 验证入口）、各仓约定（`## 涉及仓约定` 下每仓一个 `### <仓>` 小节）。启动时随根 `CLAUDE.md` 整份进持久桶。**加/减仓 = 加/减一个 `### <仓>` 小节**，与 `repos.tsv` 的 compdb 标签各司其职（一个管"进不进上下文"、一个管"进不进 compdb"）。
    
-   **`.claude/hooks/load-feature.sh` —— 注册在 SessionStart 上的脚本**：读当前分支后**只做一件事**——把树根 `CLAUDE.md` 软链（`ln -sfn`）重指到 `features/<分支>/CLAUDE.md`（根若还是真实文件则先备份、防误删），再打一行 banner。启动时 Claude Code 加载根 `CLAUDE.md`（穿软链）即把该 feature 全部上下文进持久桶。**它不再往任何仓写文件、不再 stdout 注入正文**。
    

**启动时就跑这么一条链**（工程师在树根敲 `claude` 那一刻）：

features/ 独立仓SessionStart hookClaude Code工程师features/ 独立仓SessionStart hookClaude Code工程师permissions.ask 门禁同时生效clangd + feature 精简 compdb 就绪在树根启动 claude触发 load-feature.sh读锚定仓链当前分支frameworks/base → frameworks/native → …ln -sfn 把树根 CLAUDE.md 软链重指到features/dev-sidebar/CLAUDE.md加载根 CLAUDE.md（穿软链）→ 该 feature 全部上下文(树级约束 + 总览 + 各仓约定) 进 Memory files 持久桶睁眼即整份 feature 上下文在场、抗压缩、子代理也吃到

一步步拆开：

1.  触发 SessionStart hook `load-feature.sh`：从 stdin 的 JSON 拿到 cwd，再到**锚定仓**读当前 git 分支名（读出 `dev-sidebar`）。
2.  脚本**只做一件事**：`ln -sfn` 把树根 `CLAUDE.md` 软链重指到 `features/dev-sidebar/CLAUDE.md`（根若还是真实文件先备份，防误删）。
3.  Claude Code 加载树根 `CLAUDE.md`（穿软链）→ 该 feature 的**全部**上下文（树级 bootstrap/硬约束 + 总览 + 各仓约定）一次性进 **Memory files 持久桶**。
4.  同一时刻，`permissions.ask` 护栏生效、clangd + feature 精简 compdb 就绪。
5.  于是 agent "睁眼"就握有整份 feature 上下文——在做 `dev-sidebar`、涉及哪几个仓、每个仓有什么坑、验证跑哪个脚本，且**抗压缩、子代理也吃到**，全程无需人工交代。

几个容易踩的细节都已加固：

-   **SessionStart 不写 matcher = 四种触发源全覆盖**（startup / resume / clear / compact）。初版只覆盖 startup+compact，`/clear` 之后 hook 不重跑、软链可能停在上一个 feature。
-   **每条 prompt 跑一次分支漂移检测**（UserPromptSubmit hook `check-branch-drift.sh`）：比对当前分支与快照，会话中途 `repo checkout` 切了分支会收到一次告警，提示**重启会话让 hook 把根软链重指到新 feature**；没切则零输出、零打扰。（软链只在 SessionStart 重指，中途切分支必须重启才生效——这是 v2 相对"实时注入"的一个已知取舍。）
-   **锚定仓做成链**：feature 不一定碰 frameworks/base，按 base → native → 下一候选的顺序找到第一个能读出分支的仓。
-   **软链重指幂等 + 根真实文件保护**：`ln -sfn` 重复指向同一目标无副作用；若树根 `CLAUDE.md` 曾是真实文件（如迁移前那版），hook 先 `cp` 备份再软链，绝不静默覆盖内容。**全程只动树根一个游离软链、不往任何仓写字节**。

> **博客视角的一个补白**：官方博客认为 hooks _最有价值_的用法并不是"拦住 agent 做错事"，而是**让配置自我改进**——用 stop hook 在会话结束、上下文还热的时候反思本次教训并**提议更新 CLAUDE.md**；用 start hook 动态加载团队/模块上下文，让每个人无需手动配置就拿到对的地基。我们当前的三个 hook（SessionStart 软链重指、UserPromptSubmit 分支漂移检测、PostToolUse compdb 时效补漏）已经落地了"动态加载 + 防错提醒"这半边，但还都停在"加载 + 防错"这一侧；"会话末反思 hook"已列入演进方向（第十一节），是把这套 harness 从"静态地基"推向"自我改进闭环"的下一步。

### [6.2 命门矛盾与破局：为什么 features/ 是"树根上的独立 git 仓"](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_6-2-%E5%91%BD%E9%97%A8%E7%9F%9B%E7%9B%BE%E4%B8%8E%E7%A0%B4%E5%B1%80-%E4%B8%BA%E4%BB%80%E4%B9%88-features-%E6%98%AF-%E6%A0%91%E6%A0%B9%E4%B8%8A%E7%9A%84%E7%8B%AC%E7%AB%8B-git-%E4%BB%93)

上面这套结构里最不显然的一步，是"为什么 `features/` 要单独做成一个**放在树根的 git 仓**，而不是就放进 `frameworks/base` 里、让它跟着分支走"。因为这里藏着一个 git 语义层面的死结：

与需求「不污染 gerrit」直接冲突

需求 — 上下文文件内容随 feature 分支切换

git 语义 — 文件必须被该 project 跟踪  
（被跟踪才会随 checkout 变内容）

一旦被 frameworks/base 等 project 跟踪

repo upload / gerrit 可见 → 污染上游

.git/info/exclude 只能让文件不被跟踪  
——那它就不随分支变了，两头堵死

破局 — 把「随分支」的逻辑整个搬出 project git

features/ 独立 git 仓放树根  
树根非 git 仓，无嵌套冲突  
不在 manifest，repo/gerrit 全不管  
无 Android.bp，不参与编译

目录名 = 分支名

SessionStart hook 读锚定仓当前分支  
把树根 CLAUDE.md 软链重指到对应 feature

即：「随分支变」与「不进 gerrit」在同一个 project 仓内**不可兼得**，必须把"随分支"这件事从 git 跟踪机制里拿出来，改由 hook 在会话启动时按当前分支把树根 `CLAUDE.md` 软链重指到对应 feature（这正是 6.1 那条链）。

> **为什么 features 用独立仓，而不是 `.git/info/exclude`**：exclude 只让文件不被跟踪、内容并不随 `checkout` 变，而 features 上下文**需要随分支变**——所以它必须活在一个"分支能带着它变内容"的地方，即树根的独立 features 仓 + hook 按分支重指软链。（早期方案曾在各涉及仓里用 exclude 隔离物化出来的子目录 `CLAUDE.md`；v6 取消了子目录 CLAUDE.md，这处仓内 exclude 也随之退场——现在全程只动树根一个游离软链，不往任何仓写字节。）

方案推演走了五版：

| 版本 | 方案 | 结局 |
|-----|---------------------------------------------------------------------------------|-----------------------------------------------------------------|
| v1 | feature 目录与源码树同级，在 feature 目录启动 | ✗ CLAUDE.md 只沿 cwd 树向上加载，同级源码树的上下文完全加载不到 |
| v2 | `features/` 放树内、独立 git 仓 | ✓ 部分成立，但深层单仓的约定仍进不来 |
| v3 | 根 CLAUDE.md 随分支切换 | 根不是 git 仓、"分支"是 per-project 的——要么建瘦仓手动同步（两套分支），要么 hook 动态注入 |
| v4 | **SessionStart hook 按分支注入** | ✓ 单一分支源、自动跟随、零手动同步 |
| v5 | 大单仓约定也进 `features/`，**物化成各仓 CLAUDE.md 按需加载** | ✓ 启动只注入索引 + 涉及仓清单；单仓详情动到该仓才由子目录 CLAUDE.md 送达——但注入/按需加载的内容**易失**（见下） |
| v6 | **砍掉 stdout 注入 + 子目录 CLAUDE.md，全部内联进单个 `features/<分支>/CLAUDE.md`，树根 `CLAUDE.md` 软链指向它** | ✓ 启动即整份进 Memory files **持久桶**——抗压缩、子代理也吃到；机制塌缩成"按分支重指一个软链" |

最终形态 = v6。**从 v5 到 v6 的关键转折是一个实测发现**：v5 的两条送达路——SessionStart hook 的 stdout 注入、子目录 CLAUDE.md 的按需加载——内容进的都是**会话消息流**，长会话触发上下文压缩时会被**摘掉（易失）**，而且**都不进子代理**（子代理只加载 CLAUDE.md 层、不吃 hook 注入）。真正抗压缩、且子代理也加载的，只有 **cwd 的 project memory（根 `CLAUDE.md`）里的内容**。于是 v6 把一个 feature 的全部上下文塌进单个 `features/<分支>/CLAUDE.md`、让树根 `CLAUDE.md` 软链到它——feature 上下文从此**常驻持久桶**，还顺手填平了"hook 注入不进子代理"这个缺口（见第十节表）。代价只是根 `CLAUDE.md` 变大，但那本就是每会话必用的上下文，值。

### [6.3 三个设计决策](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_6-3-%E4%B8%89%E4%B8%AA%E8%AE%BE%E8%AE%A1%E5%86%B3%E7%AD%96)

**为什么把全部上下文内联进单文件，而不是"分两条路"省上下文？** 早期（v5）确实分过两条路——启动注入"概览"、动到某仓才按需加载该仓"详情"——图的是"没碰到的仓其约定一字不占上下文"这份经济性。但一个实测发现推翻了这个取舍：**那两条路（hook stdout 注入 + 子目录 CLAUDE.md 按需加载）内容进的都是"会话消息流"，长会话触发压缩时会被摘掉（易失），且都不进子代理**。也就是说，省下来的上下文"省了，但也随时会丢、子代理还根本拿不到"——对一个要贯穿整个 feature 开发、还要派子代理测绘的场景，这个省法得不偿失。

v6 于是反过来：**把一个 feature 的全部上下文（含各仓约定）全塞进单个 `features/<分支>/CLAUDE.md`、常驻根 `CLAUDE.md`（Memory files 持久桶）**。代价是根 CLAUDE.md 变大（一个 feature 3–8 个仓的约定，几 k token 量级），换来**抗压缩、子代理也吃到、机制极简（软链一个文件）**。承接 4.1 的业务事实——涉及仓有限（3–8 个），这个"变大"可控；真正无界的是整棵树上千个仓，那从来不会全进 CLAUDE.md。所以 v6 不是放弃了上下文经济性，而是认清了：**对 feature 上下文这种"每会话必用、必须持久、必须给子代理"的东西，常驻才是对的经济性。**

**为什么全部上下文（含硬约束）放根 CLAUDE.md 而不是 hook？** 承上——**子代理不吃 hook 注入的内容，但会加载 CLAUDE.md**。"改 public API 必须跑 `m update-api`，否则 checkapi 挂构建"这种不知道会出事故的规则，必须让派出去的子代理也看见；v6 把整份 feature 上下文（硬约束 + 总览 + 各仓约定）都经软链放进根 CLAUDE.md，正好让子代理**连各仓约定一起吃到**（v5 时子代理只吃得到写在根里的硬约束、吃不到 hook 注入的 feature 概览与各仓约定，是个缺口——v6 顺手填平了它）。同时遵守一条纪律：**CLAUDE.md 只写 agent 默认会犯的错，不写文档**。本树的硬约束一共六条：

| 硬约束 | 防的是什么 |
|--------------------------------------------|-----------------------------------|
| 不向任何 gerrit project 提交 harness/上下文文件 | 知识污染上游 |
| 禁配 Java LSP / 禁生成 Eclipse 工程文件 | 吃内存 + 写坏树（见踩坑精选） |
| 改 public/System API 后必须 `m update-api` | checkapi 挂构建 |
| 新增系统服务必须同步 `system/sepolicy` | 服务起不来（avc denied） |
| push framework.jar/services.jar 后注意 ART 缓存 | dexpreopt/boot image 校验不一致拖慢甚至起不来 |
| 不手改 `out/` 下任何生成物 | 增量构建被破坏 |

**为什么编译约定也要写进 CLAUDE.md？** 有两个 agent 默认必错的点：envsetup 必须用 bash（工具默认 shell 可能是 zsh），且 `source` 后不能接 pipe（函数会进子 shell）；一切编译必须后台跑 + 轮询日志（agent 的前台命令有超时上限，而单编模块十几分钟起步）。

```perl
bash -c 'source build/envsetup.sh >/dev/null 2>&1 \
  && lunch aosp_cf_x86_64_phone-trunk_staging-userdebug >/dev/null 2>&1 \
  && m services' > /tmp/build.log 2>&1 &     # 后台 + 日志轮询，
                                             # 看到 build completed successfully 才算完
```

（顺带一个 Android 17 的新变化：lunch 目标是三段式 product-release-variant，release 段如 `trunk_staging`，可从 `out/soong.log` 的 `TARGET_RELEASE=` 反查。）

___

## [七、第③层 流程：动到哪片代码，就知道怎么编译验证](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%B8%83%E3%80%81%E7%AC%AC3%E5%B1%82-%E6%B5%81%E7%A8%8B-%E5%8A%A8%E5%88%B0%E5%93%AA%E7%89%87%E4%BB%A3%E7%A0%81-%E5%B0%B1%E7%9F%A5%E9%81%93%E6%80%8E%E4%B9%88%E7%BC%96%E8%AF%91%E9%AA%8C%E8%AF%81)

> ▶ **Demo**：[`aosp-harness-demo`](https://github.com/yuandaimaahao/aosp-harness-demo) 里对应 `.claude/skills/build-services-jar/SKILL.md`（`paths: frameworks/base/services/**`）与 `build-sepolicy/SKILL.md`（`paths: system/sepolicy/**`），配合 `features/dev-sidebar/frameworks-base.md` 里"一句话指回 skill"的单一事实源写法。

**这一层做什么。** 一句话：让 agent 一动某片代码，就自动知道这片代码"怎么编译、产物在哪、要 push 哪些文件、怎么验证"——不用人每次交代，平时也不占上下文。这类"改了这片代码该怎么走完编译到验证"的知识是**过程性知识**（区别于第②层"在哪、做什么"那种背景知识）。

**怎么做到的。** 难点在于：把它全塞进根 CLAUDE.md，会每个会话常驻、白白挤占上下文。解法是 **skill 按需加载 + `paths` glob 按路径激活**——把每一类代码的编译验证流程各写成一个 skill，并在 frontmatter 里用 `paths` 标注它作用的代码路径；只有当 agent 真的 Read 到匹配路径的文件时，对应 skill 才被拉进上下文，其余时间零占用。

paths glob 命中

paths glob 命中

paths glob 命中

path-scoped rule

Read frameworks/base/services/\*\*

build-services-jar skill 激活  
单编目标 / 产物路径 / push 清单 / ART 缓存坑

Read system/sepolicy/\*\*

build-sepolicy skill 激活  
service\_contexts + .te 三件套流程

Read frameworks/native/services/inputflinger/\*\*

build-inputflinger skill 激活

Read Android.bp / Android.mk

compdb 时效提醒 — 记得重跑刷新脚本

平时不读这些路径

这些流程知识零上下文占用

```yaml
# .claude/skills/build-services-jar/SKILL.md（frontmatter 示意）
---
name: build-services-jar
description: 编译/部署 services.jar——改 frameworks/base/services 下代码时用
paths:
  - "frameworks/base/services/**"
---
```

这里的 **glob** 就是文件路径的通配符匹配（和 shell 里 `ls *.c` 的 `*` 同源）：`*` 匹配单层路径内的任意字符，`**` 跨目录递归匹配任意层级。所以 `frameworks/base/services/**` 的意思是"`frameworks/base/services/` 目录下任意深度的任意文件"。skill frontmatter 里的 `paths` 字段就是给这个 skill 挂一组这样的路径模式；agent 每 Read 一个文件，Claude Code 就拿该文件路径逐条比对这些模式，**一旦命中就把对应 skill 载入上下文，不命中就当它不存在**。这就是它按"你正在动的代码落在哪个目录"来决定加载哪个 skill——比 hyperdroid 那种按关键词触发的粒度更准（路径是确定的，关键词会误触），也正是官方博客说的 skill "可 path-scoped、只在相关目录激活"。

repo 工程有个特殊决策点：skill 放哪。嵌套进 `frameworks/base/.claude/skills/` 会被该 gerrit project 跟踪——又是污染问题。结论与 feature 工作流同构：**集中放树根 `.claude/skills/`（树根不属于任何 project），用 `paths` glob 做作用域**。前提是从树根启动 agent，而编译/adb 本来就要求树根 cwd，天然满足。

另一条重要纪律是**单一事实源**：初版曾在 skill 和 feature 单仓约定里把编译/push 流程写了两遍，评审时判定必然漂移。最终分工——skill 承载不随 feature 变的通用流程（编译命令、产物、push 清单、已知坑）；`features/` 单仓约定只写 feature 特有内容，流程一句话指回 skill。本树目前有五个这样的 skill：framework.jar、services.jar、inputflinger、边栏 app、sepolicy，每个都写明"后台编译 + 日志轮询、产物与 push 清单、快环稳环、编过≠改对指向 verify 脚本"。

___

## [八、第④层 护栏与验证：斩断"编过 = 改对"](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%85%AB%E3%80%81%E7%AC%AC4%E5%B1%82-%E6%8A%A4%E6%A0%8F%E4%B8%8E%E9%AA%8C%E8%AF%81-%E6%96%A9%E6%96%AD-%E7%BC%96%E8%BF%87-%E6%94%B9%E5%AF%B9)

> ▶ **Demo**：[`aosp-harness-demo`](https://github.com/yuandaimaahao/aosp-harness-demo) 里对应 `.claude/settings.json` 的 `permissions.ask` 门禁与 `features/dev-sidebar/verify-sidebar.sh --demo`（四步确定性断言、`FAIL>0` 非零退出）。

**这一层做什么。** 一句话：在 agent 干活的两个危险时刻各设一道拦截——**动设备/动源码树前**别让它误操作，**收工宣布完成前**别让它把"编译通过"当成"改动正确"。前三层帮 agent 把事做顺，这一层专门防它把事做砸或自我误判。

**怎么做到的。** 两道护栏，都不依赖模型的记性：一是 **permissions 硬门禁**（`permissions.ask`），把危险命令变成系统级弹窗，模型上下文再长也绕不过（见 8.1）；二是 **确定性验证脚本**（verify 脚本），把"改对了没有"编码成只输出 PASS/FAIL 的机器判定，堵死 agent 拿模糊输出自我安慰的路（见 8.2）。

### [8.1 permissions 硬门禁](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_8-1-permissions-%E7%A1%AC%E9%97%A8%E7%A6%81)

"动设备先确认"写在文档里是软约束，模型上下文一长就可能被忽略。`permissions.ask` 把危险操作变成**系统级弹窗**，不依赖模型记性：

| 门禁命令 | 为什么要拦 |
|------------------------------------------------------|------------|
| `adb push` / `adb reboot` / `adb remount` … | 直接改写运行中设备 |
| `cvd start` / `cvd stop` / `cvd reset`、`launch_cvd` / `stop_cvd` | 虚拟机生命周期操作 |
| `repo sync` | 动整棵源码树 |
| `m clean` / `m installclean` | 清掉数小时的构建产物 |

### [8.2 确定性验证脚本](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#_8-2-%E7%A1%AE%E5%AE%9A%E6%80%A7%E9%AA%8C%E8%AF%81%E8%84%9A%E6%9C%AC)

AOSP 没有"改完跑一下"的现成测试套，**verify 脚本就是这个 feature 的测试**。utzcoz 的原话："没有这个，会话结尾 agent 会因为 build 成功就确信改动生效了。" 验证环编码成脚本、只编码一次，输出只有确定性的 PASS/FAIL——中间态会诱导 agent 把模糊输出读成成功。

否

是

起不来 / 诡异

是

否

是

改代码

单编模块  
后台跑 + 日志轮询

build completed  
successfully?

快环 — adb push 进运行中的 Cuttlefish + 重启进程  
（此处触发 permissions 弹窗确认）

设备能正常起来?

稳环兜底 — m 整机  
cvd stop → cvd start 换新镜像  
必要时清 /data/dalvik-cache/

跑 features/dev-sidebar/verify-sidebar.sh

机器判定全部 PASS?

才允许宣布完成  
编译成功 ≠ 改动正确

以 `dev-sidebar` 的 verify 脚本为例，它做四步确定性断言：

1.  `sys.boot_completed=1`（设备真的起来了）；
2.  `system_server` 存活；
3.  crash buffer 扫描（无新增崩溃）;
4.  新增系统服务与边栏应用存在性（`service list` / `pm list packages` 命中）。

feature 早期允许部分断言标 SKIP，随开发推进逐项转为硬断言——这本身就是一种可执行的进度表。Cuttlefish 作为目标设备在这里显出独特价值：**虚拟机的"稳环"（整机镜像 + `cvd stop/start`）是真机没有的兜底手段**，push 出诡异状态时可以低成本回到干净基线。

___

## [九、串起来：一个会话的完整生命周期](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E4%B9%9D%E3%80%81%E4%B8%B2%E8%B5%B7%E6%9D%A5-%E4%B8%80%E4%B8%AA%E4%BC%9A%E8%AF%9D%E7%9A%84%E5%AE%8C%E6%95%B4%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F)

四层不是四个孤立的配置，而是按时间协同的一条流水线：

每个会话循环

启动时  
根 CLAUDE.md(软链)整份载入  
「在哪、做什么、各仓约定」

干活时  
路径激活 skill  
「怎么编、怎么验」

危险动作  
permissions 拦一道  
系统级弹窗

收工前  
verify 脚本判定  
「真的改对了」

| 层 | 触发时机 | 管什么 |
|---------|------------------|-----------------------|
| ① 代码智能 | 启动 / Read C++ 文件 | 按 symbol 而非文本导航 |
| ② 上下文 | 启动 + 每条 prompt | 每会话自动知道"在哪、做什么、什么不能碰" |
| ③ 流程 | Read 到匹配路径 | 动到哪片代码就知道怎么编/push/验证 |
| ④ 护栏与验证 | 危险命令 / 收工前 | 防误操作设备、斩断"编过=改对" |

**换 feature 的成本约等于零**：`repo start dev-next --all` + 建 `features/dev-next/` 目录（含单文件 `CLAUDE.md` 与 `repos.tsv`）+ 重启会话，hook 自动把树根 `CLAUDE.md` 软链重指到 `features/dev-next/CLAUDE.md`、compdb 仓集也按 `repos.tsv` 自动切换；skills 完全不用动（paths 是按仓的，不随 feature 变）。`features/` 是独立 git 仓，feature 上下文、上游调研、verify 脚本随开发演进提交，还可以推私有 remote 跨机同步。

一句话总结这套工作流：

> **启动时 hook 告诉 agent"在哪、做什么"，干活时路径激活 skill 告诉它"怎么编怎么验"，危险动作被 permissions 拦一道，收工前 verify 脚本判定"真的改对了"——四层各管一段，缺一层闭环就断。**

___

## [十、这条路是怎么探索出来的](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%8D%81%E3%80%81%E8%BF%99%E6%9D%A1%E8%B7%AF%E6%98%AF%E6%80%8E%E4%B9%88%E6%8E%A2%E7%B4%A2%E5%87%BA%E6%9D%A5%E7%9A%84)

前面九节呈现的是**结果**——四层各就各位、彼此咬合，读起来像是一开始就照着蓝图搭的。真实过程要曲折得多：这套 harness 不是自顶向下设计出来的架构，而是被一个个具体的失败逼出来的。每一层的定型几乎都走同一条轨迹——**先撞上一个具体故障 → 挖到根因 → 才沉淀出对应的设计决策**，顺序恰恰和成品的呈现顺序相反。

回头看，整个探索大致是**四场"战役"加一轮自审**：前两场在代码智能层，分别啃下 C++（compdb 空数组的上游坑）和 Java（论证后刻意不配 LSP）两块硬骨头；第三场在上下文层，解开"随分支 ⇔ 被跟踪 ⇔ 污染 gerrit"这个 git 语义死结；第四场把流程知识从"全塞 CLAUDE.md"改造成 path-scoped skill。四层搭齐后我们没有就此收工，而是专门做了一轮自审——**"配上了"不等于"能用"**，回头把六个会在真实会话里咬人的缺陷逐个补掉。这段历程里最有价值的往往不是最终方案，而是**为什么放弃了另一些看似更直接的方案**（比如 Java LSP、把 harness 做成 plugin）。

下面这张时间线先给全局，随后的「踩坑精选」再挑几个最有代表性、且都有实证的坑展开细节。

# [AOSP 整机源码 Harness 工程探索](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html/#aosp-整机源码-harness-工程探索)

第一战 C++ LSP标准 compdb在部分基线生成空数组根因挖到 AOSP上游临时 commit定型两段式 feature精简 compdb第二战 Java LSPaidegen已被官方弃用jdtls 遍历整树吃2GB 内存论证后刻意不配，沉淀选型判据第三战 上下文组织发现「随分支 ⇔被跟踪 ⇔污染」死结五版方案推演hook + features独立仓破局第四战 流程知识全进 CLAUDE.md会常驻挤占path-scoped skill集中树根 + pathsglob 作用域一轮自审三大件搭完不等于能用修掉 6个会咬人的缺陷长编译超时、上下文静默丢失、软约束不可靠探索历程 — 四场战役与一轮自审

### [踩坑精选（每个都有实证）](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E8%B8%A9%E5%9D%91%E7%B2%BE%E9%80%89-%E6%AF%8F%E4%B8%AA%E9%83%BD%E6%9C%89%E5%AE%9E%E8%AF%81)

**坑一：Eclipse/jdtls 残留会写坏源码树。** 一次 `m` 在 aconfig 阶段失败——`flag already declared`，而 `frameworks/base` 零本地提交。根因：此前 Java LSP 实验中 Eclipse 把资源文件连同 `.class` 拷进了 `services/*/bin/`，其中的 `.aconfig` 副本被 soong 的 `**/*.aconfig` glob 收进同一 aconfig\_declarations，导致重复声明。更隐蔽的是 **frameworks/base 自带 .gitignore 忽略 `.project`/`.classpath`，`git status` 完全看不见这批残留**；甄别要用 `git ls-files --error-unmatch` 逐个判断是否 tracked（AOSP 一些老仓的 Eclipse 文件是 checked-in 的，不能一刀切删）。这让"禁配 Java LSP"从性能取舍升级成了硬约束——它不只吃内存，还会污染构建。

**坑二：机器级 clangd 包装脚本劫持（多树共存必踩）。** `clangd --check` 日志里出现了另一棵树的 compdb 路径 + "Compile command inferred" + 错误的目标架构——树根 `.clangd` 明明配对了。根因：`~/.local/bin/` 里给旧树写的 clangd 包装脚本硬编码了 `--compile-commands-dir`，而 **CLI flag 优先级压过一切 `.clangd` 配置**。修复：包装脚本按 `$PWD` 分树分发，每棵树用各自的 in-tree clangd + 各自 compdb 绝对路径；树根 `.clangd` 一律写绝对路径双保险。速查信号：`--check` 日志里 ①DB 路径不是本树 ②"inferred"字样 ③目标架构/API 级不对。

**坑三：`--query-driver` 在 AOSP 上是反作用。**（详见第五节——宿主 glibc 头污染 bionic，删掉即清零。）

**元教训：harness 自身也要用工程标准对待**——设计完要评审、加固、实测，而不是"配上了"就算完。三大件搭好后我们专门做了一轮"这套方案还有哪些缺陷"的自审，修掉的六个缺陷里最典型的三个：长编译撞工具超时上限（改为强制后台+轮询）、`/clear` 后上下文静默丢失（matcher 全覆盖 + 漂移检测）、"动设备先确认"是软约束（升级为 permissions 硬门禁）。

___

## [十一、边界与下一步](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%8D%81%E4%B8%80%E3%80%81%E8%BE%B9%E7%95%8C%E4%B8%8E%E4%B8%8B%E4%B8%80%E6%AD%A5)

**已知边界**（都是明确认下的取舍，不是遗漏）：

| 边界 | 说明 |
|----------------------|----------------------------------------------------------------------------------|
| Java/Kotlin 无 LSP | 跨 Java↔JNI↔native 追踪靠 Grep 搜符号 + JNI 注册名锚点 |
| 单树单分支 | repo 树没有 git worktree 等价物，并行两个 feature 需要两棵树 |
| ~hook 注入不进子代理~（v6 已解决） | v6 把整份 feature 上下文放进根 CLAUDE.md（软链），子代理加载 CLAUDE.md 即全部吃到；不再需要派发时转述（v5 的这个缺口已消除） |
| compdb 有时效 | 改构建文件/repo sync 后要重跑刷新脚本（已有 rule 自动提醒） |

**何时重审**：每 3–6 个月、或新一代模型发布后感觉规则见顶时，删过期/矛盾的规则——"为迁就某代模型缺陷写的规则，下一代模型上来就变成束缚"。另加一条我们自己的信号：**依赖的工具链出现弃用声明时立即重审**（aidegen 之鉴）。

**可演进方向**（按杠杆排序）：

1.  **五段式 handoff 交接文档**（What Was Done / How Verified / Files Modified / Blocker / Next）——跨会话 bug hunt 的最高杠杆，新会话读最新 handoff 即可冷启动续上；
2.  **会话末反思 hook**——提议更新 CLAUDE.md，形成持续改进闭环；
3.  **冷启动 review 子代理对抗审查**——作者 agent 偏 "ship it"，无历史包袱的 reviewer 偏 "explain this"。
4.  **只读子代理测绘、主代理编辑**——博客点名的 subagent 核心模式：整机树上"探索"极烧上下文，可先派一个**只读 subagent** 去测绘某个子系统（跟调用链、读 dumpsys、定位改动点），把发现**写进一个文件**，主代理再带着全貌下手编辑，避免探索的中间产物挤占主上下文。v6 后子代理经根 CLAUDE.md（软链）已拿到 feature 的**全部**上下文（总览 + 各仓约定），派发时无需再转述——v5 时"hook 注入不进子代理"那条边界已消除。

___

## [结语](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E7%BB%93%E8%AF%AD)

这套 harness 工程没有任何一处依赖"更聪明的模型"：它做的全部事情，是把一位 AOSP 老工程师带新人时会做的四件事——**给他一张地图（上下文）、教他怎么编译（流程）、告诉他哪些红线不能碰（护栏）、要求他改完必须验证（闭环）**——翻译成了 agent 基础设施。模型每半年换代一次，而这棵树上的地基，每个会话都在复用。

## [参考资料](http://ahaoframework.tech/agentic-coding/AOSP%E6%95%B4%E6%9C%BA%E6%BA%90%E7%A0%81Harness%E5%B7%A5%E7%A8%8B%E6%8E%A2%E7%B4%A2.html#%E5%8F%82%E8%80%83%E8%B5%84%E6%96%99)

-   本文配套可跑 Demo：[yuandaimaahao/aosp-harness-demo](https://github.com/yuandaimaahao/aosp-harness-demo)（四层方案的最小可运行复刻，关键脚本带 `--demo`，无需真实 AOSP 树，`./run-demo.sh` 一键演示）
-   Anthropic 官方博客：_How Claude Code works in large codebases_（agentic search 的代价与甜区、"上下文是唯一稀缺资源"、五扩展点 CLAUDE.md/hooks/skills/plugins/MCP + LSP + subagents 及其**叠加顺序**、hooks 的"自我改进"用法、subagents"探索与编辑分离"、"子目录初始化"建议、plugins 分发反部落化、LSP 最高杠杆、每 3–6 个月重审）
-   utzcoz：_Using Claude Code on AOSP-scale projects_（harness engineering 十模式，https://utzcoz.github.io/2026/04/26/using-claude-code-on-aosp-scale-projects.html）
-   Claude Code 官方文档：hooks / skills / memory / large-codebases（SessionStart 注入机制、`paths` frontmatter 语义、CLAUDE.md 加载规则）
-   AOSP 树内文档：`build/soong/docs/compdb.md`；相关源码 `build/soong/cc/compdb.go`、`build/soong/cc/cc.go`
-   社区同类方案（见第三节）：Lightrion AOSP RAG（https://lightrion.com/docs）、hyperb1iss/hyperdroid-skill（https://github.com/hyperb1iss/hyperdroid-skill）、jonaschen/Android-Software（https://github.com/jonaschen/Android-Software）