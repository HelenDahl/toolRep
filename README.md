发布时间：2025 年 4 月 28 日

**截止日期****：****2025** **年** **5** **月** **13** **日 23:59:59**

请注意在截止日期前提交到对应仓库！

助教：赵熙（email： fly0307@sjtu.edu.cn）

仓库地址：https://github.com/SJTU-IPADS/OS-Course-Lab/tree/OS-Pre-Course-CSAPP

## **Introduction**

在这个 lab 中，你将会使用课堂上所提及的 socket 编程，结合 [llama.cpp](https://github.com/ggml-org/llama.cpp) 的对话模型实现一个 socket server。本 lab 一共分为 5 个部分，当你全部完成之后，将能够在浏览器中与大模型进行对话。

**注：该实验全部从头设计且第一次发布，如对实验或文档中任何地方有疑问，欢迎向助教****（赵熙，****email: fly0307@sjtu.edu.cn****）****提出。**

请你切换到实验文件夹根目录，然后通过下面的命令从 GitHub 拉取 lab 代码：

```Bash
#!/bin/bash
# pull codes from GitHub
git switch OS-Pre-Course-CSAPP
git pull

# remove redundant files
ls | grep -v SocketLab | xargs -i rm -rf {}

# move lab files to the root of the project
mv ./SocketLab/* ./SocketLab/.* .
rm -rf ./SocketLab

# commit to our git server
git switch -c lab9
git add --all
git commit -m "initialize lab9"
git push -u origin lab9
```

Socket Lab 的文件结构如下所示：

```Bash
.
├── CMakeLists.txt
├── examples            # example files, to show how to use the library
│   ├── CMakeLists.txt
│   ├── json.c          # JSON library
│   └── llama.c         # LLaMA library
├── frontend            # frontend, used to present a website in the browser
│   ├── Makefile
│   ├── node_proxy      # this part will be introduced later
│   └── website         # the compiled website (written in Vue 3)
├── grade               # files used to grade your lab
│   ├── client_test.py              # Part 1: Client
│   ├── common_exceptions.py
│   ├── common.py
│   ├── concurrency_test.py         # Part 3: Concurrency
│   ├── connect.js                  # Part 4: WebSocket Access
│   ├── epoll_test.py               # Part 3: Epoll Server
│   ├── grade.py                    # Grade All-in-One
│   ├── Makefile
│   ├── message_model.py
│   ├── package.json
│   ├── package-lock.json
│   ├── server_test.py              # Part 2: Server
│   └── std_answer.py               # Part 4: Standard Answer
├── include             # header files
│   ├── config.h
│   ├── json            # our JSON library
│   ├── llama           # our LLaMA library
│   └── server.h
├── lib                 # library files
│   ├── CMakeLists.txt
│   ├── json            # our JSON library
│   └── llama           # our LLaMA library
├── Makefile
├── models
│   ├── model.gguf      # the model file
│   └── README.md
├── ref                 # client and server Python implementation for reference
│   ├── client.py
│   ├── server.py
│   └── message_model.py
├── requirements.txt    # python dependencies you need to install first
├── src                 # the main part of the lab
│   ├── client.c
│   ├── Makefile
│   ├── server.c
│   └── server_epoll.c
└── thirdparty          # third-party libraries
    ├── json            # JSON library
    └── llama.cpp       # LLaMA library
```

## **Part 0: Preparation**

在 lab 开始之前，需要一些准备工作。请完成接下来的几步，这样你将对这个 lab 有一个初步的了解。

### **Step 1: Initialize the Git Submodule**

首先，希望你了解什么是 Git Submodule，以及如何使用。在这个 lab 中，我们需要用到两个第三方库——一是用来实现自定义格式的 JSON 库，二是 llama.cpp 的源码。众所周知，C/C++ 没有统一的包管理器（不像 Python 有 pip，Rust 有 cargo），因此我们需要将这些库文件放置在我们的项目中统一管理。当然，我们可以选择直接将这两个项目的源码拷贝至项目下然后加入到 Git 提交中，但是这样会使得项目变得臃肿/不清晰。因此，我们希望你能够学会如何使用 Git Submodule。

Git Submodule 是指在一个 Git 项目中嵌套另一个 Git 项目作为子模块，具体的规则写在 `.gitmodules`文件中。

```Plain
[submodule "thirdparty/json"]
        path = thirdparty/json
        url = https://github.com/nlohmann/json.git
[submodule "thirdparty/llama.cpp"]
        path = thirdparty/llama.cpp
        url = https://github.com/ggml-org/llama.cpp.git
```

如上图所示，可以看到我们的项目里有两个子模块，这个文件里存储了它们的 URL 和需要被 clone 到的路径，事实上，还可以存有更多的内容——比如要 checkout 到的哈希值，要切换到的分支等等。通常来说，为了防止库文件迭代更新造成兼容性问题，submodules 往往会加上需要 checkout 到的哈希值，但是本次实验使用的就是最新的提交，因此这里并没有添加。

对于一般的 Git 项目来说，`.gitmodules`文件仅仅保存元数据，而真正注册的模块保存在`.git`文件夹中随着项目被拉取而同步。但是我们的实验发布将 lab 文件放在仓库某个子文件夹中（`SocketLab`），这将使得删除重组重新提交后子模块的注册规则失效了（比如 llama.cpp，原先路径可能是`SocketLab/thirdparty/llama.cpp`，但是迁移之后变成了`thirdparty/llama.cpp`，原先的注册信息失效）。因此在本 lab 中目录下仅保留了`.gitmodules`，在完成了文件的迁移之后，你需要通过在项目根目录执行下面的命令行脚本来拉取子模块：

```Bash
#!/bin/bash
git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | while read path_key path_val; do
    url_key=$(echo $path_key | sed 's/\.path/.url/')
    url_val=$(git config -f .gitmodules --get "$url_key")
    git submodule add $url_val $path_val
done
```

对于更一般的包含子模块的项目，为了初始化项目并拉取子模块，可以执行以下命令（本次实验中不用，但是如果以后你重新克隆自己的仓库并切换到这个 lab 分支，需要以下命令来拉取子模块）：

```Bash
$ git submodule update --init --recursive
```

### **Step 2: Install Lab Dependencies**

在这个 Lab 中，使用到了 cmake 来编译整个项目，另外用到了 Python 来对代码进行测试，因此我们需要安装 cmake 和 Python 环境：

```Bash
$ sudo apt update && sudo apt install cmake python3 python3-pip python3-dev -y
$ pip install -r requirements.txt
```

等待安装完成之后，就可以使用 Python 来对代码进行测试了。

对于某些在 Debian/Ubuntu 上使用系统级 Python 的同学，在执行上面的第二行时，如果不使用 Virtual Envirenment 或者 Anaconda ，可能无法直接使用 `pip` 在 `site-packages` 中添加需要的包。对于这种情况，可以通过以下`apt`命令安装：

```Bash
$ sudo apt install python3-nest-asyncio cython3 -y
```

> 如果安装cython3后依然编译报错提示cython: not found，可执行下述指令让 cython 指向 cython3
>
> ```Bash
> bash which cython3#查看cython3路径
> sudo ln -s <上步中cython3路径> /usr/local/bin/cython#让 cython 指向 cython3
> #或者执行sudo ln -s <上步中cython3路径> /usr/bin/cython
> bash which cython #验证是否能找到cython
> ```

### **Step 3: Get LLM Model**

这个 lab 中使用到了 llama.cpp 作为我们的推理框架，首先需要获取到用来推理的大模型。我们用来测试的模型是 [qwen2.5-coder-1.5b-instruct-q8_0.gguf](https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF/blob/main/qwen2.5-coder-1.5b-instruct-q8_0.gguf)，你也可以选择任一 llama.cpp 支持的模型，但无论如何请将其**重命名为** **`model.gguf`**然后放置在 `models/`目录下方便后续测试。

```Bash
$ wget -O models/model.gguf https://104.244.46.244/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF/resolve/main/qwen2.5-coder-1.5b-instruct-q8_0.gguf?download=true
```

如果上述下载模型因网络原因超时，可以从这个[链接](https://ipads.se.sjtu.edu.cn:1313/d/d64dbab552d04e3586dc/)下载：

```Bash
$ wget -O models/model.gguf "https://ipads.se.sjtu.edu.cn:1313/d/d64dbab552d04e3586dc/files/?p=%2Fqwen2.5-coder-1.5b-instruct-q8_0.gguf&dl=1"
```

*在完成这个 lab 之后，你可以尝试下载一些更大的模型（上面的这个参数数量是 1.5B），替换之后测试效果，体会不同的模型参数数量给生成结果带来的差异。*

### **Step 4: Compile Examples**

由于这个 lab 和以前涉及的 lab 存在一定的不同——使用 CMake 来对这个项目进行管理，且引入了第三方库，让我们先来简单热个身。

首先回忆一下 C 语言和 C++ 语言的区别：C 语言是面向过程的编程语言，而 C++ 是面向对象的。它们主要的区别就在于 C++ 支持类型系统及其继承，而 C 甚至没有`std::string`，这就导致手写某些处理函数变得非常繁琐。相比之下，C++ 中的标准库如`std::vector`和`std::map`等都是非常方便的数据结构，也因此 llama.cpp 很好地利用了 C++ 的速度与便利的优势，在保持推理速度的同时也不会造成代码过于繁琐。

C++ 需要依赖于 libstdc++、libc，而操作系统层次并不再拥有这些库，因此在操作系统编程中，使用得更多的语言还是 C，也可以看到本课程实验使用的主要语言还是 C。而在本 lab 中需要将 C 语言和 C++ 语言结合起来，即需要在 C 程序里面（要实现的`server`和`client`）调用 C++ 的库（如 LLaMA 库），这就要求 C++ 提供给 C 的函数接口必须能够在 C 中体现——不能包含 `std::vector`等等（因为 C 中并没有这些类型）。方便起见，我们为 JSON 库和 LLaMA 库进行了一层封装（在`lib/`文件夹下），你需要遵循这个库文件的调用规则（参考`include/`目录下的头文件接口声明），考虑如何在你的 C 代码中使用这两个库。

可以看到，在`examples/`目录下有两个文件，`json.c`和`llama.c`，我们提供这两个文件作为调用的例子。接下来请你编译这个项目，执行：

```Bash
$ mkdir build
$ make build
```

这将会在 `build/`目录下配置 CMake 并进行编译。`json.c`和`llama.c`会分别被编译到`build/examples/json_example`和`build/examples/llama_example`的位置。

接下来尝试运行这两个可执行文件（下面的命令是在项目根目录执行的示例，如不在根目录，请自行根据可执行文件和模型文件的路径进行修改），分别执行下述指令：

```Bash
$ ./build/examples/json_example
$ ./build/examples/llama_example -m ./models/model.gguf
```

运行后输出内容如下：

```Bash
$ ./build/examples/json_example
class: ICS
semester: 2024-2025-2
$ ./build/examples/llama_example -m ./models/model.gguf
............................................................................
User llama_client_1 asks: 请记住我是ICS助教。
Response: 当然，我会记住你是ICS助教。
The current response has ended

User llama_client_1 asks: 请问我是谁？
Response: 我是ICS助教。
The current response has ended

User llama_client_2 asks: 请问我是谁？
Response: 我是人工智能助手，没有个人身份。
The current response has ended
```

你需要查看`json.c`和`llama.c`的源码，了解如何调用这两个库，在这个过程里，可以加深对 C 语言面向过程的特性的认识。这里对`llama.c`的源码流程作一定的解释：

```c
static void quest_wrapper(int conn, const char *client_name,
                          const char *message) {
    // Log the message to the console
    printf("User %s asks: %s\n", client_name, message);
    printf("Response: ");

    // Call the quest function to get the response
    quest_for_response(conn, client_name, message);
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    if (argc != 3 || strcmp(argv[1], "-m") != 0) {
        fprintf(stderr, "Usage: %s -m <model_path>\n", argv[0]);
        return 1;
    }

    // Example usage of the LLaMA CLI
    if (initialize_llama_chat(argc, argv) != 0)
        return 1;

    // Add clients
    const char *client_names[] = {"llama_client_1", "llama_client_2"};
    for (int i = 0; i < sizeof(client_names) / sizeof(client_names[0]); ++i) {
        if (add_chat_user(client_names[i]) != 0) {
            fprintf(stderr, "Failed to add client: %s\n", client_names[i]);
            free_llama_chat();
            return 1;
        }
    }

    // Execute the quest function for the client
    quest_wrapper(0, client_names[0], "请记住我是ICS助教。");
    quest_wrapper(0, client_names[0], "请问我是谁？");

    // Execute the quest function using another client
    // The second client will not be able to see the first client's messages
    quest_wrapper(0, client_names[1], "请问我是谁？");

    // Free the llama chat resources
    for (int i = 0; i < sizeof(client_names) / sizeof(client_names[0]); ++i) {
        if (remove_chat_user(client_names[i]) != 0) {
            fprintf(stderr, "Failed to remove client: %s\n", client_names[i]);
        }
    }

    // Free the llama CLI resources
    free_llama_chat();
    return 0;
}
```

在`main`函数中，首先解析命令行参数获取到模型文件，接着初始化模型，然后向 llama 注册了两个不同的用户（两个用户会具有不同的上下文）。接着调用封装好的`quest_for_response`函数进行推理（在`quest_wrapper`函数中），这个函数会根据传入的`client_name`查找对应的上下文，然后根据该上下文进行推理，并将推理结果放回到上下文中以供下一次推理。最后请不要忘记释放资源——先将注册过的用户所用的上下文移除，然后释放大模型占用的资源。

首先需要说明的是：这个测试并没有进行任何 socket 的连接或信息的发送，`llama.c`中的两个函数`send_token`和`send_eog_token`在 LLaMA 库中被调用，这两个函数可以在下面的 **Part 2: Implement the Server Using Multi-Threading Way** 中找到详细解释。这个示例里我们仅仅将各 token 拼凑在一起放在`buffer`数组中，并在每次生成结束时调用的`send_eog_token`中打印一行字符。而在后面的实现 server 步骤中，你需要在这两个函数里实现 socket 发送过程，包括封装以及发送，这样才能真正进行 socket 通信。

另外，可以看到`llama.c`中存在两个用户先后向大模型提问，他们根据不同的名字进行区分——上面的回复中也可以看到，第一个用户告诉了模型他的身份，而第二个用户并没有，于是第二个用户的回答与第一个用户关于“我是谁”的回答不同。因此你需要妥善管理不同的用户——关于保存不同用户上下文内部的处理我们已经实现好了，你只需要始终保持同一个用户使用同一个用户名即可，**那么如何保证用户名不发生冲突会是这个 lab 中你需要思考并解决的一个问题。**

### **Step 5: Test Example Client and Server**

为了方便测试，我们以 Python 形式给出了示例和测试所用的 client 和 server（不包括第三部分使用 epoll 实现的 server），请你先执行 client 和 server 进行简单的测试与体会。

我们首先启动 server，请注意，server 命令行参数需正确指定模型的位置：

**若在本地性能较差的虚拟机中启动** **server** **可能时间较长，如果启动时间过长，为避免** **4** **个** **part** **测试超时或测试不稳定，可在本地暂时修改测试脚本** **grade** **目录下各****`open_client`****函数中的超时重试间隔，当前默认为** **1s**

```Bash
$ python3 ./ref/server.py -m ./models/model.gguf
............................................................................
The server has been started, listening on: 127.0.0.1:12345
```

接着在新的终端启动 client 与 server 进行连接：

```Bash
$ python3 ./ref/client.py
Connected to 127.0.0.1:12345
> 
```

输入测试问题“你好!”，可以看到 client 和 server 侧进行流式回复：

```Bash
$ python3 ./ref/server.py -m ./models/model.gguf
............................................................................
The server has been started, listening on: 127.0.0.1:12345
Client connected from: ('127.0.0.1', 44266)
Received ('127.0.0.1', 44266): {"token": "\u4f60\u597d!", "eog": true}
你好！有什么我可以帮忙的吗？
$ python3 ./ref/client.py
Connected to 127.0.0.1:12345
> 你好!
你好！有什么我可以帮忙的吗？
> 
```

你在下面的三个部分中需要实现的效果应该与这里的类似。

注意：请尽量**使用英文进行对话**，因为 C 语言在处理 UTF-8 中文字符的时候可能会遇到截断等问题，导致 JSON 库出现解析报错。当然，一般情况下是没有问题的。

**若在本地性能较差的虚拟机中启动 server 可能时间较长，如果启动时间过长，为避免 4 个 part 测试超时或测试不稳定，可在本地暂时修改测试脚本 grade 目录下****`open_client`****函数中的超时重试间隔，当前默认为 1s**

## **Part 1: Warmup: Implement the Client**

在这个部分，你需要实现一个 client 用于与 server 进行通信。先思考一下 client 和 server 的不同点：client 只需要进行简单的发送请求，然后接收来自 server 的回复，而 server 则可能会面对着很多的 client 同时请求，那么如何保证资源竞争中不发生冲突，以及尽可能让 client 的请求得到及时的响应，这就是 server 需要解决的一个问题。而 client 相对更简单，只需要实现一个单线程的 socket 发送接收即可。

在`config.h`中，我们设定了几个常量供你使用（除非发生端口占用冲突，否则不用修改）：

```C
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 12345
#define MAX_BUFFER_SIZE 1024
```

另外需要说明的是，server 通过 socket 向 client 发送的消息格式是 JSON，请你调用相关库对这个 JSON 格式的字符串进行反序列化从而获取到其中的 token 并打印在终端上。使用的 JSON 格式如下：

```JavaScript
{
    "token": "",
    "eog": true
}
```

`token`是为了和 server 进行流式回复时的发送格式相对应，在 client 侧，发送给 server 的 JSON 中，`token`字段应该包含单次提问的 prompt，而`eog`字段应该设置为`true`。

在代码中我们提供了较多的注释，请你参考代码中的注释，结合课上所学，完成 client 这部分代码。你所需要实现的 client 需要具备提供的 Python client 所具有的功能。在完成之后，你可以使用下面的命令来测试你的 client：

```Bash
$ make grade-part1
```

这将使用提供的 Python client 和你实现的 client 分别与提供的 Python server 进行交互，对比它们的输出是否一致。你的 client **连接上 server 之后需要打印一行以 "connected" 开头的信息**如`Connected to 127.0.0.1:12345`，这样测试程序才能判断你与 server 建立了连接，才会继续进行下一步评测。如果正确实现`client.c`，你将会看到类似于下面的输出：

```Bash
$ make grade-part1
python3 grade/client_test.py
connect failed: Connection refused
............................................................................
Client Output:     > **Hello! How can I assist you today?**
Reference Output:  > **Hello! How can I assist you today?**
Final Score:       20
```

注：*上面的**`connect failed`**不需要 care，因为 server 启动需要一定的时间，所以测试中会不断尝试连接到 server，只要最终**输出**是正确**，得到评分**即可。**另外，请不要使用 Hack 的方式通过直接发送固定的输出来通过这个实验，**评分**测试的时候会使用不同的 Prompt* *及**不同的模型 :)*

## **Part 2: Implement the Server Using Multi-Threading Way**

在这个部分，你需要实现一个 server，用于接收 client 的请求，并返回对话补全的结果。现在请你回想课堂上所讲到的三种实现并发 server 的方式——**多进程、多线程、IO 多路复用**。由于多进程和多线程在实现上相差无几，并且创建进程的开销要大于创建线程，因此在这个 lab 中我们不进行多进程方式的实现。这个部分希望你使用**多线程**的方式来实现 server。

server 部分的结构相对较复杂，这里先介绍一下`server.h`中相关的两个函数：

```C
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void send_token(int conn, const char *token);
void send_eog_token(int conn);

#ifdef __cplusplus
}
#endif
```

这两个函数在 LLaMA 库中被调用，也是你需要实现的一部分。

LLaMA 中相关的调用如下：

```C++
    // 下面的循环用来执行推理
    while (true) {
        // ......这里省略了很长一部分代码

        // convert the token to a string, print it and add it to the response
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0,
                                     true);    // 将输出的token转化成word
        if (n < 0) {
            GGML_ABORT("failed to convert token to piece\n");
        }
        std::string piece(buf, n);
        printf("%s", piece.c_str());    // 打印出当前token到终端
        fflush(stdout);
        response += piece;              // 拼凑到response中

        // send the piece to the client
        send_token(chat_user.conn, piece.c_str()); // 函数调用，将当前token发送到client里

        // prepare the next batch with the sampled token
        batch = llama_batch_get_one(&new_token_id, 1);
    }

    // send the EOG token to the client
    send_eog_token(chat_user.conn);    // 函数调用，发送结束符到client，告知client当前对话生成完毕
    return response;
```

这里不断生成 token 直到遇到`eog`（end of generation），你需要使用 JSON 库对这个消息进行封装，然后发送到 client，因此这两个函数就成为**封装**和**发送**的关键。

在这个 part 中，你将简单地为每一个 accept 的 client 创建一个线程，然后在这个线程中不断处理这个 client 的请求，直到 client 或 server 断开连接。在完成`server.c`之后，你可以使用下面的命令来测试你的 server：

```Bash
$ make grade-part2
```

如果实现无误，你将会看到类似下面的输出：

```Bash
$ make grade-part2
python3 grade/server_test.py
connect failed: Connection refused
............................................................................
connect failed: Connection refused
............................................................................
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Final Score: 30
```

## **Part 3: Implement the Server Using IO Multiplexing Way**

在这个部分，你需要实现一个 server，用于接收 client 的请求，并返回对话补全的结果。这个部分希望你使用 IO 多路复用的方式来实现 server。你需要修改的文件是 `src/server_epoll.c`，其他文件如 Part 2 中所述，下面我们简单介绍一下 `src/server_epoll.c`。

在课堂上，我们讲到了 IO 多路复用的 select 方式，但是 select 相对显得更繁琐且可读性不强，而且现在的 IO 多路复用编程也基本上都由 select 和 poll 转向了 epoll。因此在这个 part 中，我们希望你使用 epoll 来实现 IO 多路复用，在测评时，我们会创建很多个仅连接而不发送任何请求的 client，此时你的 server 具有的线程数必须等于`1`。**注意：**由于评测并不检查具体实现逻辑，你也可以使用 poll 进行实现，但是请不要使用 select 方式，因为测试中的 client 总数已经超过了 select 方式的最大监听数量（Linux 上通常为 1024）。

接下来我们简要介绍一下 IO 多路复用的优势。在 Part 2 中，你已经实现了一个多线程的 server，这个 server 会对每一个 client 创建一个线程，然后在这个线程中处理这个 client 的请求。但是这样的方式会带来一些问题，如果有很多的 client 同时建立连接，但是并不对 server 发送数据，那么这些创建出来的线程就一直闲置着。尽管在 Unix 系统中，闲置的线程并不会占用太多的资源，但是创建/销毁线程仍然有一定开销。而 IO 多路复用的方式则可以解决这个问题，在这个方式中，我们只需要一个线程来监听所有阻塞的文件描述符，当有文件描述符就绪时，创建一个线程来处理这个文件描述符。这样就可以做到**按需创建**，对资源的利用更加高效。

接下来我们再简单介绍一下 epoll 实现的事件驱动型 IO 多路复用。epoll 是 Linux 下的一种 IO 多路复用机制，它可以监听多个文件描述符，当其中有文件描述符就绪时，它会以事件的形式通知你。epoll 有三个主要的函数：`epoll_create1`、`epoll_ctl`和`epoll_wait`。

- `epoll_create1`接收一个参数`flag`（不需要考虑，直接传入`0`即可），返回 epoll 实例的文件描述符。该函数用于创建一个 epoll 实例。
- `epoll_ctl`接收四个参数，第一个参数是前面用`epoll_create`创建出的 epoll 的文件描述符；第二个参数是表明向 epoll 实例中添加（`EPOLL_CTL_ADD`）还是删除（`EPOLL_CTL_DEL`）文件描述符；第三个参数是要添加/删除的文件描述符，而第四个参数是指向一个事件结构体（`struct epoll_event`）的指针，添加时设置`{ .events=EPOLLIN, .data``.fd``=fd }`，而删除时置空即可。该函数用于向 epoll 实例中添加/删除文件描述符。(如果设置了 `EPOLLIN` 标志，这意味你希望在该文件描述符fd变为可读状态时得到通知)
- `epoll_wait`接收四个参数，第一个参数是前面用`epoll_create`创建出的 epoll 的文件描述符，第二个参数是一个事件结构体（`struct epoll_event`）的数组，第三个参数是数组的大小，第四个参数是超时时间（由于本 lab 中 client 会在不确定的时间发送消息，没有建立心跳机制，故将该参数设置为`-1`），返回值`n`是就绪的文件描述符的个数，表明第二个参数的事件结构体数组的前`n`个元素均为 ready 的事件。当该值为`0`时，表明不需要进行处理，应该进入下一次等待，当该值大于`0`时，需要从`0`到`n`遍历事件数组，查看哪一些事件亟待处理，而如果它小于`0`，说明出现了错误，比如用户键入`CTRL + C`中断 server。该函数用于等待文件描述符就绪。

接下来我们希望你思考使用 epoll IO 多路复用的方式来实现 server 的一整套流程，这里我们提供一个参考流程：

1. 创建一个 epoll 实例。
2. 打开一个 socket，绑定到一个端口上，然后监听这个 socket。
3. 将这个 socket 添加到 epoll 实例中。
4. 进入一个循环，不断调用`epoll_wait`，等待文件描述符就绪。
5. 当有文件描述符就绪时，遍历事件数组，查看哪一些事件亟待处理。
   1. 如果是监听 socket 就绪，表明有新的 client 连接，那么你需要接受这个连接，并将这个 client 的 socket 添加到 epoll 实例中。
   2. 如果是 client 对应的 fd 就绪，表明这个 client 发送了消息，那么你需要接收这个消息，创建新线程调用推理函数，从而返回对话补全的结果。
6. 重复 4-5 步，直到 server 被中断。

请根据代码中给出的注释，开始你的 epoll 之旅吧！在实现之后，你可以使用下面的命令来测试你的 server：

```Bash
$ make grade-part3
```

如果你的实现正确，你将会看到下面的结果：

```Bash
$ make grade-part3
python3 grade/epoll_test.py
connect failed: Connection refused
............................................................................
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
<此处省略非常非常多“Connected to 127.0.0.1:12345”>
Final Score: 30
```

**非root用户运行，****如果出现报错****`[Errno 24] Too many open files`****，可以执行****`ulimit -n unlimited`****(或****`ulimit -n 2048`****)****将单个进程的 fd 数量限制改为无限制**

完成 Part 3 后，请运行集成测试（大约3到4分钟）：

```Bash
$ make grade
```

如果实现正确，你将会看到类似下面的输出：

```Bash
$ make grade
python3 grade/grade.py
<此处省略CMake Build Log>
connect failed: Connection refused
............................................................................
............................................................................
client_test.py: 20
server_test.py: 30
concurrency_test.py: 20
epoll_test.py: 30
Final Score: 100
```

接下来我们看一下 concurrency_test，这个测试中我们会尝试建立很多空连接——如果还是如 Part 2 这样的实现，建立 1024 个空连接会创建 1024 个线程，相应地会带来非常大的开销。你可以查看这部分测试的源码，可以了解到测试使用的方法最终比较的是运行时间差。比如：

```Bash
$ python3 ./grade/concurrency_test.py
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
<此处省略非常非常多“Connected to 127.0.0.1:12345”>
Time Difference: 2.209076404571533
Final Score:     20
```

可以看到时间多创建 1024 个 client 带来的时间差仅为 2.2s，但是如果修改 `concurrency_test.py`的第95行，改为使用普通的一对一创建，我们就可以看到明显的效率之差了：

```Diff
diff --git a/grade/concurrency_test.py b/grade/concurrency_test.py
index 744c782..ef76d5e 100644
--- a/grade/concurrency_test.py
+++ b/grade/concurrency_test.py
@@ -92,7 +92,7 @@ def main() -> None:
         time.sleep(0.1)
 
     # open the C server
-    server_process = open_server(subprocess_server_epoll)
+    server_process = open_server(subprocess_server)
     client_process = open_client(subprocess_client_ref)
 
     # write "Hello" to stdin of the client
```

改为调用 Part 2 中的 server 进行相同的测试：

```Bash
$ python3 ./grade/concurrency_test.py
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
Connected to 127.0.0.1:12345
<此处省略非常非常多“Connected to 127.0.0.1:12345”>
Time Difference: 84.80091166496277
Final Score:     0
```

建议你动手尝试一下，相信到这里你能够对 IO 多路复用有一个更深的理解。

## **Part 4: Website Demonstration**

在这一部分，我们会将我们实现的 server 真正接入到浏览器中。为了后面进行测评，请你先运行下面这条命令生成评测标准答案`std_answer.txt`：

```Bash
$ make prepare-part4
```

接下来我们来了解一下前端部分——我们为这个大模型对话实现了一个前端，你可以在浏览器中输入对话，然后通过 socket 与 server 进行通信，最后得到大模型生成的响应。这个前端的文件结构如下：

```Bash
.
└── frontend            # frontend, used to present a website in the browser
    ├── Makefile        # Makefile
    ├── node_proxy      # accept the request from the browser and send it to the server
    └── website         # the static page to be presented in the browser
```

这里先对这个项目结构进行一个简单的说明：为了能够在前端展示，已经使用 Vue 3 编写了一个前端网页，编译完成之后放在`frontend/website`文件夹下。但由于现代浏览器的限制，建立 socket 连接之前必须进行 http 握手，这就导致使用 C 语言编写的 server 无法直接与前端进行通信。因此已经在`frontend/node_proxy`文件夹下实现了一个简单的 Node.js 代理服务器，这个代理与前端进行 socket 通信，然后将前端的请求转发给 C 语言编写的 server，最后将 server 的回复返回给前端。这样一来，我们就可以在浏览器中输入对话，然后得到对话补全的结果。

**注**：如果你使用的是 Docker（如我们课程最开始所述），请注意，Docker 默认启动时使用“桥接”的网络模式，这会在容器内部创建一个相对独立的网络。因此你可能需要启动一个新的 Docker 容器（添加`--network host`参数，即“镜像”的网络模式，该模式会直接使用宿主机的网络），然后在这个容器中执行下面的操作。或者你可以选择在主机上安装 Node.js 和 Python，然后同样在主机上执行下面的命令，但是需要如下的额外一些操作，请看下面的解释。

如果你前面的 Lab 均在 Windows 上使用 WSL 2 完成，那么配置使用镜像模式将会变得很简单。你需要做的事情是，在用户目录下（通常来说是`C:\Users\<your-user-name>`），创建一个文件 `.wslconfig` ，向其中写入以下内容：

> ```
> [wsl2]
> networkingMode=mirrored
> ```

执行 `wsl --shutdown` 重启 WSL 后，即可以镜像网络模式启动 WSL 2 ，然后跟随后面的步骤做即可。

如果你前面的 Lab 均在 Windows 上使用 Docker 完成，那么很遗憾， Windows 上的 Docker Engine 已经移除了“**为每个容器给定一个 IP**”的特性（如果你感兴趣的话，可以查看这个 [GitHub Issue](https://github.com/docker/roadmap/issues/93) ）。那么为了完成这部分工作，你可能还是需要重新启动一个容器，并在启动时指定`--network host`来使用 Host 网络，或者映射端口`-p 5175:5175`来将容器中的端口映射到主机上。下面我们提供一些命令供参考：

> ```
> docker run --name socket_server -p 5175:5175 -it ics bash
> sudo apt update
> sudo apt install cmake make python3 python3-pip nodejs npm -y
> ```

（在另一个终端中）

> ```
> docker cp <your_lab_folder> socket_server:/home/ics/
> docker exec -it socket_server bash
> cd /home/ics/<your_lab_folder>
> ./build/src/server
> ```

（再在另一个终端中）

> ```
> docker exec -it socket_server bash
> cd /home/ics/<your_lab_folder>
> make middleware
> ```

然后在浏览器中访问[网站](http://202.120.40.8:10680/courses/ics/labs/socketlab/)，或者在 Host 机器上执行下面的第三步（如果没有安装`make`，可以将 Makefile 中的命令复制出来运行）即可。

如果你前面的 Lab 是在 Linux/Mac 上使用 Docker 完成，并且容器网络采用桥接模式，你可以选择在主机上安装 Node.js 和 Python，然后你需要在代理服务器配置中进行一些修改。在`frontend/node_proxy/src`文件夹下，有一个`proxy.js`文件，你需要修改其中的`TCP_SERVER_HOST`字段，将其修改为你的容器的 IP 地址（可以查看[这篇文章](https://www.howtogeek.com/devops/how-to-get-a-docker-containers-ip-address-from-the-host/)）。然后你需要在`include`文件夹下的`config.h`文件中修改`SERVER_ADDR`字段，将其由`127.0.0.1`修改为`0.0.0.0`，这样你的 server 将会监听所有的 IP 地址（此时主机访问容器经过 Docker Bridge，通常为`172.17.0.1`，如果仍然仅监听`127.0.0.1`的话，会拒绝掉来自`172.17.0.1`的连接）。然后你需要重新编译并在容器中启动 server：`./build/src/server -m ./models/model.gguf`，其余步骤和下面的相同（在 Host 宿主机上执行剩余步骤）。

下面我们介绍直接**在 Linux** **宿主机****上**，**或者采用镜像模式****（添加****`--network host`****参数）****启动****的****容器****中**所需要进行的操作：

1. 安装 Node.js，因为代理服务器使用 Node.js 编写，并需要`npm`来安装依赖。
   1. `sudo apt install nodejs npm -y`
   2. 注：事实上，apt 仓库中的 Node.js 版本非常陈旧，但是由于这个代理服务器的代码非常简单，旧版本的 Node.js 也可以运行。
2. 执行`make middleware`，这会安装代理服务器的依赖，并启动代理服务器。
   1. ```Bash
      $ make middleware
      make -C frontend middleware
      make[1]: 进入目录“./frontend”
      make -C node_proxy
      make[2]: 进入目录“./frontend/node_proxy”
      node src/proxy.js
      WebSocket server started on port 5175
      New WebSocket client connected.
      Connected to TCP server.
      ```
3. 另外一个终端中执行`make ``website`，这将会调用 Python 的`http.server`模块，在`8000`端口启动一个简单的 http 服务器，接着访问`http://localhost:8000`，你将会看到一个简单的网页，你可以在这个网页中输入对话，然后得到对话补全的结果。
   1. ```Bash
      $ make website
      make -C frontend website
      make[1]: 进入目录“./frontend”
      python3 -m http.server --directory website
      Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...
      ```

   2. 注：第 3 步并不必须，因为我们已经在服务器上部署了一个前端，你可以直接访问[网站](http://202.120.40.8:10680/courses/ics/labs/socketlab/)来查看而不必在本地部署。但是注意，第 2 步的**代理服务器**中间件必须在本地部署。
4. 启动本地的 server，然后刷新网页，在网页中输入 prompt，查看模型生成的对话。

![img](https://ipads.feishu.cn/space/api/box/stream/download/asynccode/?code=NzQ0MzkyZmJkMjc1ODI3MDk3MTczZmQzMDcyN2EyNDVfWU1JNG1HYm1SckNkUHdGc25EM1FBMFA2OHRFckNNSXpfVG9rZW46QTVQemJNRWZNb1RHVVF4b21XV2M5N3hPbmNjXzE3NDcxMDYzMzg6MTc0NzEwOTkzOF9WNA)

接下来对你的前端部署进行评测，将用 Node.js 模拟前端发送信息，然后与`std_answer.txt`进行比较。请执行下面的命令进行评测：

```Bash
# 在宿主机或者使用镜像网络（添加--network host参数）的容器中执行
$ make grade-part4
```

如果你的操作正确，将可以看到类似于下面的输出：

```Bash
$ make grade-part4
make -C grade grade-nodejs
make[1]: Entering directory 'grade'
node connect.js
Connected to the server.
Message sent: { token: 'Hello', eog: true }
**Hello! How can I assist you today?**
End of message received.
Connection closed.
Final score: 100
make[1]: Leaving directory 'grade'
```

## Submit Instruction

注意，这个实验中你需要提交的代码仅包含`src/`文件夹中的三个源文件，我们在评测时也只会使用这三个文件。

```Bash
#!/bin/bash
# track files, commit changes and push them
git add ./src/*
git commit -m "lab9 has been completed"
git push
```

> ![img](https://ipads.feishu.cn/space/api/box/stream/download/asynccode/?code=ZGRjNjk0NzZlNWVjMTU0YzlhNTYzNGE1MzIwNzJlYTZfcHAwVGZvWk96dmk5Y2Vwd1Z5MnpVNFl5YW5USENZTkdfVG9rZW46UmZEcWIyaTdub2c3Z0Z4SGJNM2NlV0dlblBnXzE3NDcxMDYzMzg6MTc0NzEwOTkzOF9WNA)
>
> 有多位同学反映测试时用户两次发送同样内容🤯但模型返回结果不同
>
> Reference 0utput得到这样输出的实际原因为：
>
> 1. 每次创建新的用户调用模型时，因为没有上下文且固定随机种子，不同的用户用相同prompt提问模型，回复会是一样的；
> 2. 同一个用户提问，第一次的输出是用户第连接后第一次问题请求的输出，有的Prompt首次回复带了星号，第二次的输出没有带星号因为模型利用了同一个用户的上下文做回复，所以不带星号。
>
> 如果与Reference 0utput不一致，请同学检查自己的实现是否正确。