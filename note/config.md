# config 配置模块
配置模块简而言之就是将需要配置的参数写入配置文件中，然后在程序中读取配置文件，从而实现程序的解耦， 我们选择的配置文件是yaml，这样我们就可以简化程序。

```c++
比如我想添加写入文件的Appender，并设置日志格式，在程序中我需要这样做
sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
file_appender->setFormatter(fmt);
file_appender->setLevel(sylar::LogLevel::ERROR);
logger->addAppender(file_appender);
```
好家伙，不写注释估计很难看懂这是在干什么，而且这样做修改就显得极为麻烦。
变成yaml格式后，我们只需要这样做
```c++
loggers:
    - name: root
      level: DEBUG
      formatter: "%d%T%p%T%m%n"
      appenders:
        - type: FileLogAppender
          level: ERROR
          file: ./log.txt
```
这样是不是瞬间就清晰了很多，而且修改起来也很方便，这就是配置模块的好处。

废话太多了，进入正题。
## ConfigVarBase 配置变量的基类
首先明确一点什么叫做配置变量，就是我们在配置文件中写的配置项，比如上面的loggers，name，level，formatter，appenders，type，file等等，这些都是配置项，我们将这些配置项抽象成一个个的配置变量。方便管理。

```c++
基类中的核心方法
virtual std::string toString() = 0;
virtual bool fromString(const std::string& val) = 0;
```
最重要的就是这两个方法了，如何将配置项转为字符串，以及如何将字符串转为配置项，这两个方法是配置模块的核心。因为配置项的种类很多， 我们需要借助模板来实现。

## 类型转换

```c++
//F from_type, T to_type
template<class F, class T>
class LexicalCast {
public:
    T operator()(const F& v) {
        return boost::lexical_cast<T>(v);
    }
};
```
这个模板实现了从F类型到T类型的转换，比如从int到string，从string到int，从string到double等等，这个模板是基础，我们通过这个模板类型的各种偏特化实现不同类型的转换，以期达到我们配置变量的“万能转换”。

```c++
template<class T>
class LexicalCast<std::string, std::vector<T> > {
public:
    std::vector<T> operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        typename std::vector<T> vec;
        std::stringstream ss;
        for(size_t i = 0; i < node.size(); ++i) {
            ss.str("");
            ss << node[i];
            vec.push_back(LexicalCast<std::string, T>()(ss.str()));
        }
        return vec;
    }
};
```
好的我们开始特例化 首先我们想要支持stl容器类型，我们就需要将需要转换的stl类型进行特例化，以vector为例，首先加载字符串v，然后遍历node，将node中的每一个元素转为string，然后再将string转为T类型，最后将T类型的元素push到vector中，这样就实现了从string到vector的转换。

测试如下
```c++
std::string input = "[1, 2, 3, 4, 5]"; // Input string to convert
sylar::LexicalCast<std::string, std::vector<int>> cast; // Create an instance of the specialization
std::vector<int> result = cast(input); // Call the operator() function to convert the input string

// Print the converted vector
for (const auto& element : result) {
    std::cout << element << " ";
}
std::cout << std::endl;
```
结果就是我们可以得到1，2，3，4，5的输出

我们的转换是相互的，vector可以转换为string string 同样可以转换为vector所以还需要再实现一种特化
```c++
template<class T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& v) {
        YAML::Node node;
        for(auto& i : v) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
```
这样就实现了vector到string的转换，这样我们就可以将vector类型的配置项转为string类型的配置项，这样就可以实现配置项的读取和写入。

这里的转换使用了YAML::Node 类型但是我们不需要对这个太过深入，只需要知道这个类型可以作为两种类型转换的桥梁就行。

这里需要注意vector转string的结果是不同的， 不是想象中的[1, 2, 3, 4, 5]，而是yaml类型的
```c++
- 1
- 2
- 3
- 4
- 5
```
由于list类型与vector类型过分相似（就是改了个名字）这里就不再进行介绍，测试的结果和vector是一样的。
好的继续，现在我们实现set和string的转换 和 vector的转换过程及其相似，这里就不再赘述了，但是set会去除重复元素，比如输入[1,1,3]，就会
输出：
```c++
- 1
- 3
```
map 这种映射类型不太一样 我们可以看看测试结果。
```c++
std::map<std::string, int> hash;
hash["wangziyi"] = 1;
sylar::LexicalCast<std::map<std::string, int>, std::string> cast3;
cast3(hash);
std::cout << cast3(hash) << std::endl;
return 0;
```
输出的字符串就是一个键值对
```c++
wangziyi: 1
```
但是这个键值对是符合yaml文件类型的

剩下的就不再做测试了，因为都是类似的，这里需要注意的是，我们的转换是相互的，比如vector可以转为string，string也可以转为vector，这样就实现了配置项的读取和写入。

总结我们实现了对复杂类型转换的支持，stl（vector, list, set, unordered_set, map, unordered_map） 通过使用模板和对上述类型的模版特化实现复杂类型的转换，这样我们就可以实现真正的配置变量了。

## ConfigVar 配置变量

好的好的，现在到了这里我们就可以看看配置变量的类了，我们之前所做的特化操作在这里就可以派上用场了，请看这个模板定义
```c++
template<class T, class FromStr = LexicalCast<std::string, T>
        , class ToStr = LexicalCast<T, std::string> >
```
这个模板定义了三个参数，第一个代表数据类型，后两个带默认值的代表数据转换的类型，我们使用前面定义的特化类型来进行类型转换

```c++
std::string toString() override {
    try {
        //return boost::lexical_cast<std::string>(m_val);
        return ToStr()(m_val);
    } catch (std::exception& e) {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
            << e.what() << " convert: " << typeid(m_val).name() << " to string";
    }
    return "";
}

bool fromString(const std::string& val) override {
    try {
        //m_val = boost::lexical_cast<T>(val);
        setValue(FromStr()(val));
    } catch (std::exception& e) {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::toString exception"
            << e.what() << " convert: string to " << typeid(m_val).name()
            << " - " << val;
    }
    return false;
}

```
注意try语句中的内容，这里我们不在使用boost库的类型转换，而是使用我们自己定义的类型转化，这样就可以实现 stl 和 yaml string 类型的相互转换，至于fromString中外面嵌套的setValue那个是为了将修改后的配置在代码层面实现感知，说白就是修改配置后，代码也能感知到，这个后面会说到（通过回调函数实现）。


## Config 配置类

现在到了最后一个类了，这个类是配置模块的核心，我们看看这个类的私有属性
```c++
static ConfigVarMap& GetDatas() {
    static ConfigVarMap s_datas;
    return s_datas;
}
```

是一个ConfigVarMap类型，那我们再看看这是什么？
```c++
typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
```
现在明白了，这是一个unordered_map 用于映射配置项的键和值（也就是名字和配置项）

我们在类中实现了两个静态方法，用于寻找我们的配置项
```c++
template<class T>
static typename ConfigVar<T>::ptr Lookup(const std::string& name,
        const T& default_value, const std::string& description = "") {
    auto it = GetDatas().find(name);
    if(it != GetDatas().end()) {
        auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
        if(tmp) {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
            return tmp;
        } else {
            SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not "
                    << typeid(T).name() << " real_type=" << it->second->getTypeName()
                    << " " << it->second->toString();
            return nullptr;
        }
    }

    if(name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._012345678")
            != std::string::npos) {
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
        throw std::invalid_argument(name);
    }

    typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
    GetDatas()[name] = v;
    return v;
}

template<class T>
static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    auto it = GetDatas().find(name);
    if(it == GetDatas().end()) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
}

```

我们主要看看第一个方法，这个方法用于查找配置项，如果配置项存在就返回配置项，如果不存在就创建一个配置项，然后返回配置项，这样就实现了配置项的查找和创建。但是需要注意，如果我们创建的配置项名字和之前创建的配置项的名字相同，但是值的类型不同，在智能指针转换的时候就会报错（日志上的），然后返回空指针，这就防止了配置名相同，但是类型不同的情况。

先不急着往下继续测试一下上面的功能

```c++
sylar::ConfigVar<int>::ptr g_int_value_config =
    sylar::Config::Lookup("system.port", (int)8080, "system port");
 
sylar::ConfigVar<float>::ptr g_int_valuex_config =
    sylar::Config::Lookup("system.port", (float)8080, "system port");
```
我们截取测试文件的两行来说明，我们发现这两个配置项的名字相同但是类型不同，那么在进行智能指针转换的那里就会返回空，从而触发日志输出，日志输出的结果为

```
2023-05-24 17:53:52     5212    0       [ERROR] [root]  /home/wangziyi/workspace/zylar/include/config.h:327     Lookup name=system.port exists but type not f real_type=i 8080
```
表明配置名存在但是类型不同，现在的类型为int（i是缩写 float 会缩写为f...） 

好的继续，我们来测试一下Config模块

测试代码参见test_yaml.cpp 中的 test_config 函数，这里会牵扯到Config文件的另外三个方法
```c++
static void LoadFromYaml(const YAML::Node& root);
static ConfigVarBase::ptr LookupBase(const std::string& name);

static void ListAllMember(const std::string& prefix,
                          const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node> >& output)
```
第三个准确的来说不算是类中的方法，因为这个方法是静态的，但是这个方法是用于辅助第一个方法的，所以就放在一起了。其作用就是将yaml文件中的配置项，写入第三个参数list中去，而LoadFromYaml就是将ListAllMember函数写入的配置项与自己本身已有的配置项进行对比，如果配置项重名，就可以进行修改，没有的就跳过，而比较新配置是否在原配置中存在就需要依靠LookupBase来完成了， 现阶段我们可以通过LookUP 来设置我们关心的配置项，然后通过LoadFromYaml来对其进行更新，test_config的测试也就是将写入的配置修改（修改是通过加载yaml文件实现）然后打印前后配置的区别。

好的我们准备进行第三步的测试 也是配置模块的最后一个测试

现在考虑一种情况，我们在之前实现了vector的类型转换，但是vector中我们在测试的时候装的都是普通类型，假设我们需要装载自定义类型，那么我们又该如何实现类型转换呢？

```c++
class Person {
public:
    Person() {};
    std::string m_name;
    int m_age = 0;
    bool m_sex = 0;

    std::string toString() const {
        std::stringstream ss;
        ss << "[Person name=" << m_name
           << " age=" << m_age
           << " sex=" << m_sex
           << "]";
        return ss.str();
    }

    bool operator==(const Person& oth) const {
        return m_name == oth.m_name
            && m_age == oth.m_age
            && m_sex == oth.m_sex;
    }
};
```
以这个Person为例，这次彻底实现对复杂类型的支持，首先还是老方法，我们需要对之前的Lexicast模板类型实现特化（针对Person类的特化）这样才能实现对Person 和 yaml string 的类型转换。
```c++
template<>
class LexicalCast<std::string, Person> {
public:
    Person operator()(const std::string& v) {
        YAML::Node node = YAML::Load(v);
        Person p;
        p.m_name = node["name"].as<std::string>();
        p.m_age = node["age"].as<int>();
        p.m_sex = node["sex"].as<bool>();
        return p;
    }
};

template<>
class LexicalCast<Person, std::string> {
public:
    std::string operator()(const Person& p) {
        YAML::Node node;
        node["name"] = p.m_name;
        node["age"] = p.m_age;
        node["sex"] = p.m_sex;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};
```
其实也非常好理解，就是将Person类中的成员变量转换为yaml格式字符串，没了就这么简单。

好的，我们直接看测试代码中最有趣的部分
```c++

sylar::ConfigVar<Person>::ptr g_person =
    sylar::Config::Lookup("class.person", Person(), "system person");

g_person->addListener(10, [](const Person& old_value, const Person& new_value){
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "old_value=" << old_value.toString()
            << " new_value=" << new_value.toString();
});
```
我们定义了Person类的配置项，名为class.person 但是我们没有对Person中的成员变量进行初始化，所以我们可以在测试文件刚开始的日志输出中，我们Person的的值为

```python
before: [Person name= age=0 sex=0] 
- name: ""
  age: 0
  sex: false
```
但是我们指定了回调函数，并在LoadFromYaml中调用了回调函数（具体是LoadFromYaml会更新原配置，而更新操作会调用回调函数），所以我们可以看到在日志输出中，Person的值已经被修改为了我们在yaml文件中指定的值

```python
old_value=[Person name= age=0 sex=0] new_value=[Person name=sylar age=31 sex=1]
```
这样我们修改配置文件中的值，就可以在代码级别体现出来，诶呀，这样配置的基本功能就实现了，接下来是要将，配置功能和日志功能结合起来，这样我们就可以在配置文件中修改日志的级别，输出位置等等，这个就是下一节的内容了。