# MINI 词法与语法分析器

这是一个轻量的 C++17 命令行组件。

## 编译

在项目根目录执行：

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude src/main.cpp src/analyzer.cpp -o mini_analyzer.exe
```

## 运行

```powershell
# 词法分析：生成 .dyd 和 .lex.err
./mini_analyzer.exe lex sample/sample1.mini

# 语法分析：读取已有 .dyd，生成 .var、.pro 和 .parse.err
./mini_analyzer.exe parse sample/sample1.dyd

# 完整流程：词法分析后自动进行语法分析
./mini_analyzer.exe all sample/sample1.mini
```

也可以指定输出目录：

```powershell
./mini_analyzer.exe all sample/sample1.mini output
```

## 文件结构

- `include/analyzer.h`：组件接口与运行模式。
- `src/analyzer.cpp`：词法分析器、递归下降语法分析器及输出表。
- `src/main.cpp`：命令行入口。
- `sample/sample1.mini`：实验提供的完整测试程序。
- `sample/sample2.mini`：实验提供的错误测试程序。
