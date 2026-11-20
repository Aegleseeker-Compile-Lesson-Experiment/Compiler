# Aegleseeker-Compiler

![Songs_aegleseeker](./pictures/Aegleseeker.jpg)

---

## 项目描述

Aegleseeker-Compiler（[GitHub - Aegleseeker-Compile-Lesson-Experiment/Compiler · GitHub](https://github.com/Aegleseeker-Compile-Lesson-Experiment/Compiler/tree/main)） 是基于武汉大学课程《编译系统实践》实现的基于 $\mathcal{RISC-V}$ 汇编语言的编译器。编译器以课程文档 [课程总览 | 编译原理实验](https://lightsmilew.github.io/compiler-principles/docs/labs/capstone) 为参考，从头至尾基于文档、自己思考和 AI 辅助完成。编译器数千行代码均为原创实现，涵盖**词法分析**、**语法分析**、**语义分析**、**中间代码生成**、**目标代码生成**、**中间代码优化**和**目标代码优化**等编译系统核心功能：

- **词法分析**：

  极简实现。

- **语法分析**：

  实现 `LL(1)`，`SLR(1)`，`LR(1)` 和 `LALR(1)` 共四种语法分析方法，最终使用 `LALR(1)`。

- **语义分析**：

  实现简易 `AST`，支持全量报错功能。

- **中间代码生成**：

  实现 `LLVMIR` 规范生成，后续由于优化需求剔除其中 $i_{32}\to i_{1}$ 转化，但保留整体框架。

- **目标代码生成**：

  实现简易完全基于读/写内存的 `L/S` 方法完成最初目标代码生成。

- **中间代码优化**、**目标代码优化**：

  常量优化——减少寄存器读写。

  死变量优化——删去无用变量指令。

  无副作用有界空循环消除——删去无用空循环。

  `mem2reg` 优化——大幅减少内存读写。

  合并比较跳转——将 `li + bnez` 转换为通用 B 型指令。

  局部窥孔优化——删去相邻无用内存读写。

  乘除模优化——将 $2^k$ 的乘除取模转化为 `slli`、`srai`、`andi`。

---

队伍名：


$$
\mathcal{Aegleseeker}
$$


队员：

- **黄羽翔**（2024302061069@whu.edu.cn）
- **徐昊源**（）：
- **陈志业**（2815142682@qq.com）
- **孙旺宏**（2291760414@qq.com）：

## 完成情况

线上评测：

- **词法分析**：$100$分。
- **语法分析及语义分析**：$100$分。
- **代码生成及优化**：$85+12.3=97.3$分。

总计
$$
\frac{100+100+97.3}{3}=99.1
$$

## 项目结构

`main` 分支：

```
Compiler/
├── docs/                                  # 文档
├── pictures/                              # 图片与素材
├── src/                                   # 编译器源码
│   ├── Analyser/                          # 语法分析
│   │   ├── analyser.cpp / analyser.h      # 语法分析主模块
│   │   ├── init.cpp / init.h              # 初始化
│   │   ├── lalr1.cpp / lalr1.h
│   │   ├── ll1.cpp / ll1.h
│   │   ├── lr1.cpp / lr1.h
│   │   ├── slr1.cpp / slr1.h
│   │   └── tools.cpp / tools.h            # 工具函数
│   ├── Optimizer/                         # 优化
│   │   └── optimizer.cpp / optimizer.h    # 覆盖 IR 优化和汇编优化
│   ├── Sementiser/                        # LLVMIR & 代码生成 & 语义分析
│   │   ├── codegen.cpp / codegen.h        # RISC-V 代码生成与小部分优化
│   │   ├── irgen.cpp / irgen.h            # LLVMIR 生成与小部分优化
│   │   └── sementiser.cpp / sementiser.h  # 主模块, 覆盖语义分析
│   ├── Tokenizer/                         # 词法分析
│   │   └── tokenizer.cpp / tokenizer.h
│   └── main.cpp                           # 程序主入口
├── third_party/                           # 第三方组件 & 自有组件
│   ├── dots/                              # 语法分析图
│   ├── toyc/                              # 语法分析图
│   │   └── libtoyc.a                      # 第三方函数
│   └── larry                              # 文法
├── LICENSE
├── Makefile
└── README.md                              # 本文件
```

`xhy` 分支：



`czy` 分支：