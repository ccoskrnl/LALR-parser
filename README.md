# LALR(1) 语法分析器

## 介绍

语法分析是编译器前端中最复杂最核心的一个阶段。LALR技术是在实践中最常用的技术，因为用这种方法得到的分析表比规范 LR 分析表要小很多，而且大部分常见的程序设计语言构造都可以方便地使用一个LALR文法表示。

这个项目使用C++从零实现了LALR(1)语法分析器，它基于LR(0)状态集并对其运行展望符传播算法来完成LALR(1)状态集的构建，最后通过LALR(1)状态集构建ACTION表和GOTO表。

它接受一段文法描述，并基于这个文法生成一个LALR(1)分析表。这个分析表可以使用LR自动机进行语法分析。下面给出了一段文法描述示例：

```bnf
# 1. 文法的第一行被解释为文法的起始处。
# 2. 首字母大写的符号被解释为非终结符号。
# 3. 通过 '< >' 包裹的符号也被解释为非终结符号。
# 4. 一个符号只能是非终结符号或者终结符。

S -> L = R | R
L -> * R | id
R -> L
```

## 使用

设计一个优雅无冲突的文法是一个非常复杂的工作，在使用语法分析器生成器（例如Yacc/Bison）之前，请仔细检查你的文法。

下面展示了一个C语言风格的表达式文法的示例：
```
Expression -> Assignment-Expression Assignment-Expression-Trait

Assignment-Expression -> Conditional-Expression | Assignment-Expression-Prime
Assignment-Expression-Prime -> Unary-Expression Assign-Operator Assignment-Expression
Assignment-Expression-Trait -> , Assignment-Expression | epsilon
Assign-Operator -> = 

Conditional-Expression -> Binary-Expression
Binary-Expression -> Unary-Expression Unary-Expression-Trait
Unary-Expression -> Postfix-Expression | Unary-Operator Unary-Expression
Unary-Expression-Trait -> Binary-Operator Unary-Expression | epsilon

Postfix-Expression -> Primary-Expression 

Unary-Operator -> ++ | -- | & | * | + | - | ~ | !
Binary-Operator -> + | - | * | / | %
Primary-Expression -> id | int_lit | float_lit
```

它可以解析 `a = b`，`a = b + 3` 等字符串。

**有冲突的文法**

由于该LALR文法分析器生成器并不提供符号优先级的功能，所以在设计文法的时候一定要避免 `归约` 和 `移入` 冲突。

虽然改语法分析器生成器可以处理左递归和空表达式(epsilon)，但是对于移进 `规约-冲突` 仍然无能为力。考虑下面这段文法：
```bnf
AssignStmt -> id = Expr ;
Expr -> Expr AddOp Term | Term
Term -> Term MulOp Factor | Factor
AddOp  -> + | -
MulOp -> * | /
Factor -> id | int_lit | ( Expr )
```
当使用LALR进行语法分析生成时会遇到如下的错误:
```
Reduce - Shift conflict at state 7 on symbol * between REDUCE(3){[ID: 3 ]  Expr -> Term } and shift to 15
```

状态7存在一个移进-规约冲突：当遇到符号`*`时，分析器不知道是应该按照产生式`Expr -> Term`进行规约，还是应该移进`*`符号并转到状态15。

在状态7中，有以下项目：
- `Expr -> Term .` (可规约项目)
- `Term -> Term . MulOp Factor` (需要移进MulOp)
- `MulOp -> . *` 和 `MulOp -> . /` (MulOp的产生式)

当遇到`*`符号时：
1. 可以按照`Expr -> Term`进行规约
2. 也可以移进`*`符号，然后按照`MulOp -> *`进行规约


## TO-DO

- [ ] 解析正则表达式作为DFA实现Lexer
- [ ] 添加文法优先级

