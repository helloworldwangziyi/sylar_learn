# 日志模块和配置模块的整合
总算到了这里，我们需要将之前写的两个模块结合一下，为了解决需要手动配置日志的问题，我们通过配置文件配置，再通过回调函数将配置信息传递给日志模块，这样就可以大大减轻用户的负担，下面我们来看看怎么做。

首先我们需要明确配置日志需要那些配置项，比如你需要设置日志输出器的格式，日志输出器的类别（是输出到标准输出，还是输出到文件）......

我们需要好好的来考量一下我们需要的配置项，首先
```c++
日志输出器的配置
int type = 0; //1 File, 2 Stdout
LogLevel::Level level = LogLevel::UNKNOW;
std::string formatter;
std::string file;
首先你是那种类型的输出器，然后你的日志级别是多少，然后你的格式是什么，最后你的文件名是什么

日志器的配置
std::string name;
LogLevel::Level level = LogLevel::UNKNOW;
std::string formatter;
std::vector<LogAppenderDefine> appenders;
首先你的名字是什么，然后你的日志级别是多少，然后你的格式是什么，最后你的输出器有哪些
```

好的有了上面这些信息，我们就可以比较完整的配置日志了，我们需要一个配置类，这个类需要将配置文件中的信息解析出来，然后将信息传递给日志模块，这样就可以实现配置日志了，下面我们来看看怎么做。

哈哈其实就是把上面整理好的进行封装
```c++
struct LogAppenderDefine {
    int type = 0; //1 File, 2 Stdout
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::string file;

    bool operator==(const LogAppenderDefine& oth) const {
        return type == oth.type
            && level == oth.level
            && formatter == oth.formatter
            && file == oth.file;
    }
};

struct LogDefine {
    std::string name;
    LogLevel::Level level = LogLevel::UNKNOW;
    std::string formatter;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine& oth) const {
        return name == oth.name
            && level == oth.level
            && formatter == oth.formatter
            && appenders == appenders;
    }

    bool operator<(const LogDefine& oth) const {
        return name < oth.name;
    }
};
```
对比看看Person类我们就知道这么做的缘由了。

好，接下来我们就需要对上述类型进行转换，就是进行模板的特例化，以求支持将我们定义的这两个类型与yaml string进行转换。

梳理一下yaml string 转 set<LogDefine>的过程
1. YAML::Node node = YAML::Load(v); //将yaml string转换为yaml node
2. 遍历node，如果node中没有name的节点，我们就打印这个节点没有设置名字，然后跳过（日志器怎么可能没有名字？或者没有名字的日志器你敢用吗？）
3. 定义一个LogDefine类型的变量ld
4. 把name赋给ld的name
5. 把level赋给ld的level
6. 把formatter赋给ld的formatter（如果有的话）
7. 之后就是检查日志的输出其定义了吗，挨这把LogAppenderDefine类型的变量问询并赋值。
8. 问完之后把LogAppenderDefine类型的变量添加到ld的appenders中
9. 最后把ld添加到set<LogDefine>中

另外一个set<LogDefine> 转yaml string的过程，十分简单，就不写了（理解大意就行）

虽然这段转换代码很长，但是无非就是实现一个功能就是转换，我们抓住这一点去看就行

最后一个小东西就是回调函数怎么写，这个是我遇见最麻烦的问题，首先需要对比新老值的区别（这就需要知道为什么我们需要使用set来装载LogDefine了）
直接点LogDefine的name就告诉我们了，装的是root system之类的日志器定义，那为什么使用set 因为set可以去重（我们在LogDefine中重载了==操作符），比如你在日志文件中修改完又回退（等于没修改）所以新旧值相同，set会自动去重，而你修改之后和原来的就不一样了，我们就需要对修改的部分进行修改。

如果你添加了新的logger在yaml文件中你会 添加类似system这样的logger，那么set就会新增，我们就需要和older_value 进行对比如果是新增，我们就添加，如果是修改，我们就修改，如果是删除，我们就删除。

本这这样的思路，回调函数就可以看懂了，那么日志和配置模块的整合也就完成了。
